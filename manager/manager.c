#include "logging.h"
#include "config.h"
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

static char *register_pipe_name;
static char *pipe_name;
static char *box_name;
static int mode;

enum {
    CODE_BOX_CREATE = 1,
    CODE_BOX_REMOVE = 2,
    CODE_BOX_LIST = 3
};

void create_register(char *buffer) {
    int i = 0;
    char *aux_pipe_name = pipe_name;
    char *aux_box_name = box_name;
    switch (mode) {
    case CODE_BOX_CREATE:
        buffer[i++] = '3';
        break;
    case CODE_BOX_REMOVE:
        buffer[i++] = '5';
        break;
    case CODE_BOX_LIST:
        buffer[i++] = '7';
        break;
    default:
        break;
    }
    buffer[i++] = '|';
    for (; i < PIPE_NAME_SIZE && *aux_pipe_name != '\0'; i++) {
        buffer[i] = *aux_pipe_name++;
    }
    for (; i < PIPE_NAME_SIZE+1; i++) {
        buffer[i] = '\0';
    }
    switch (mode) {
    case CODE_BOX_CREATE: case CODE_BOX_REMOVE:
        buffer[i++] = '|';
        for (; i < PIPE_PLUS_BOX_SIZE+1 && *aux_box_name != '\0'; i++) {
            buffer[i] = *aux_box_name++;
        }
        for (; i < PIPE_PLUS_BOX_SIZE+2; i++) {
            buffer[i] = '\0';
        }
    default:
        break;
    }
}

/* static void print_usage() {
    fprintf(stderr, "usage: \n"
                    "   manager <register_pipe_name> create <box_name>\n"
                    "   manager <register_pipe_name> remove <box_name>\n"
                    "   manager <register_pipe_name> list\n");
} */

int main(int argc, char **argv) {
    if (argc < 4) {// || strcmp(argv[0], "manager")) {
        fprintf(stdout, "ERROR %s ; argv[0] = %s\n", "manager: need more arguments\n", argv[0]);
        return -1;
    }
    
    register_pipe_name = argv[1];
    pipe_name = argv[2];
    box_name = NULL;
    if (!strcmp(argv[3], "create")) {
        if (argc < 5) {
            fprintf(stdout, "ERROR %s\n", "manager: need more arguments");
            return -1;
        }
        mode = CODE_BOX_CREATE;
        box_name = argv[4];
    }
    else if (!strcmp(argv[3], "remove")) {
        if (argc < 5) {
            fprintf(stdout, "ERROR %s\n", "manager: need more arguments");
            return -1;
        }
        mode = CODE_BOX_REMOVE;
        box_name = argv[4];
    }
    else if (!strcmp(argv[3], "list")) {
        mode = CODE_BOX_LIST;
    }
    else {
        fprintf(stdout, "ERROR %s %s\n", "manager: argument not valid:", argv[3]);
        return -1;
    }

    int register_pipe = open(register_pipe_name, O_WRONLY);
    if (register_pipe < 0) {
        if (errno == ENOENT)
            return 0;
        fprintf(stdout, "ERROR %s\n", "Failed to open server pipe");
        return EXIT_FAILURE;
    }

    if (unlink(pipe_name) != 0 && errno != ENOENT) {
        fprintf(stdout, "ERROR unlink(%s) failed:\n", pipe_name);
        return -1;
    }
    if (mkfifo(pipe_name, 0666) != 0) {
        fprintf(stdout, "ERROR %s\n", "mkfifo failed");
        return -1;
    }
    
    ssize_t bytes_written;
    char *buffer;
    switch (mode) {
    case CODE_BOX_CREATE: case CODE_BOX_REMOVE:
        buffer = (char*) malloc(sizeof(char)*(PIPE_PLUS_BOX_SIZE+3));
        create_register(buffer);
        bytes_written = write(register_pipe, buffer, sizeof(char)*(PIPE_PLUS_BOX_SIZE+2));
        break;
    case CODE_BOX_LIST:
        buffer = (char*) malloc(sizeof(char) *(PIPE_NAME_SIZE+2));
        create_register(buffer);
        bytes_written = write(register_pipe, buffer, sizeof(char)*(PIPE_NAME_SIZE+1));
        break;
    default:
        unlink(pipe_name);
        return EXIT_FAILURE;
    }
    if (bytes_written < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to write pipe");
        unlink(pipe_name);
        return EXIT_FAILURE;
    }

    if (close(register_pipe) < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to close pipe");
        unlink(pipe_name);
        return EXIT_FAILURE;
    }

    free(buffer);
    buffer = (char*) malloc(sizeof(char)*(ERROR_MESSAGE_SIZE+4));
    int named_pipe;
    if ((named_pipe = open(pipe_name, O_RDONLY)) < 0) {
        fprintf(stdout, "%s\n", pipe_name);
        fprintf(stdout, "ERROR %s\n", "Failed to open pipe");
        unlink(pipe_name);
        return EXIT_FAILURE;
    }
    while (true) {
        
        ssize_t bytes_read = read(named_pipe, buffer, sizeof(char)*(ERROR_MESSAGE_SIZE+4));
        if (bytes_read == 0) {
            char error_message[ERROR_MESSAGE_SIZE];
            switch (buffer[2]) {
            case '0':
                fprintf(stdout, "OK\n");
                break;
            default:
                for (int i = 4; i < ERROR_MESSAGE_SIZE+4 && buffer[i] != '\0'; i++) {
                    error_message[i-4] = buffer[i];
                }
                error_message[ERROR_MESSAGE_SIZE-1] = '\0';
                fprintf(stdout, "ERROR %s\n", error_message);
                break;
            }
            free(buffer);
            unlink(pipe_name);
            return 0;
        } else if (bytes_read < 0) {
            fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
            free(buffer);
            unlink(pipe_name);
            return EXIT_FAILURE;
        }
    }
    
    /* free(buffer);
    if (close(named_pipe) < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to close pipe");
        unlink(pipe_name);
        return EXIT_FAILURE;
    }

    if (unlink(pipe_name) != 0 && errno != ENOENT) {
        fprintf(stdout, "ERROR unlink(%s) failed:\n", pipe_name);
        return -1;
    } */

    return 0;
}
