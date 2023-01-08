#include "logging.h"
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
    switch (mode) {
    case CODE_BOX_CREATE:
        buffer[i++] = '3';
        break;
    case CODE_BOX_REMOVE:
        buffer[i++] = '5';
        break;
    default:
        break;
    }
    buffer[i++] = '|';
    for (; i < 257 && *pipe_name != '\0'; i++) {
        buffer[i] = *pipe_name++;
    }
    for (; i < 257; i++) {
        buffer[i] = '\0';
    }
    buffer[i++] = '|';
    for (; i < 290 && *box_name != '\0'; i++) {
        buffer[i] = *box_name++;
    }
    for (; i < 290; i++) {
        buffer[i] = '\0';
    }
}

void create_listing(char *buffer) {
    int i = 0;
    buffer[i++] = '7';
    buffer[i++] = '|';
    for (; i < 256 && *pipe_name != '\0'; i++) {
        buffer[i] = *pipe_name++;
    }
    for (; i < 257; i++) {
        buffer[i] = '\0';
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
    
    ssize_t bytes_written;
    char *buffer;
    switch (mode) {
    case CODE_BOX_CREATE: case CODE_BOX_REMOVE:
        buffer = (char*) malloc(sizeof(char) * 291);
        create_register(buffer);
        bytes_written = write(register_pipe, buffer, sizeof(char)*290);
        break;
    /* case CODE_BOX_CREATE:
        buffer = (char*) malloc(sizeof(char) * 291);
        create_register(buffer);
        bytes_written = write(register_pipe, buffer, sizeof(char)*290);
        break; */
    case CODE_BOX_LIST:
        buffer = (char*) malloc(sizeof(char) * 258);
        create_listing(buffer);
        bytes_written = write(register_pipe, buffer, sizeof(char)*258);
        break;
    default:
        return EXIT_FAILURE;
    }
    if (bytes_written < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to write pipe");
        return EXIT_FAILURE;
    }

    if (close(register_pipe) < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to close pipe");
        return EXIT_FAILURE;
    }

    return 0;
}
/* (void)argc;
    (void)argv;
    print_usage();
    WARN("unimplemented"); // TODO: implement
    return -1; */