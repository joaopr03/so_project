#include "logging.h"
#include "config.h"
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>

//static int register_pipe;
static char *register_pipe_name;
static char *pipe_name;
static char *box_name;
static int named_pipe;

static void sig_handler(int sig) {
    if (sig == EOF) {
        if (close(named_pipe) < 0) {
            fprintf(stdout, "ERROR %s\n", "Failed to close pipe");
        }
        fprintf(stdout, "%s\n", "LOGOUT");
        if (unlink(pipe_name) != 0 && errno != ENOENT) {
            fprintf(stdout, "ERROR unlink(%s) failed:\n", pipe_name);
        }
        fprintf(stderr, "Ended successfully\n");
        exit(EXIT_SUCCESS);
    }
}

void create_register(char *buffer) {
    int i = 0;
    char *aux_pipe_name = pipe_name;
    char *aux_box_name = box_name;
    buffer[i++] = '1';
    buffer[i++] = '|';
    for (; i < PIPE_NAME_SIZE && *aux_pipe_name != '\0'; i++) {
        buffer[i] = *aux_pipe_name++;
    }
    for (; i < PIPE_NAME_SIZE+1; i++) {
        buffer[i] = '\0';
    }
    buffer[i++] = '|';
    for (; i < PIPE_PLUS_BOX_SIZE+1 && *aux_box_name != '\0'; i++) {
        buffer[i] = *aux_box_name++;
    }
    for (; i < PIPE_PLUS_BOX_SIZE+2; i++) {
        buffer[i] = '\0';
    }
}

int main(int argc, char **argv) {
    if (argc < 4) {// || strcmp(argv[0], "pub")) {
        fprintf(stdout, "ERROR %s ; argv[0] = %s\n", "publisher: need more arguments\n", argv[0]);
        return -1;
    }
    
    register_pipe_name = argv[1];
    pipe_name = argv[2];
    box_name = argv[3];

    char buffer[PIPE_PLUS_BOX_SIZE+3];
    create_register(buffer);

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
    
    ssize_t bytes_written = write(register_pipe, buffer, sizeof(char)*(PIPE_PLUS_BOX_SIZE+2));
    if (bytes_written < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to write pipe");
        return EXIT_FAILURE;
    }

    if (close(register_pipe) < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to close pipe");
        return EXIT_FAILURE;
    }

    
    signal(EOF, sig_handler);
    char buffer_input[P_S_MESSAGE_SIZE];
    while (true) {

        if ((named_pipe = open(pipe_name, O_WRONLY)) < 0) {
            fprintf(stdout, "%s\n", pipe_name);
            fprintf(stdout, "ERROR %s\n", "Failed to open pipe");
            unlink(pipe_name);
            return EXIT_FAILURE;
        }

        if (fgets(buffer_input, P_S_MESSAGE_SIZE, stdin) == NULL) {
            sig_handler(EOF);
            fprintf(stdout, "ERROR %s\n", "Failed to read from stdin");
        }
        char *aux_buffer_input = buffer_input;
        int i = 0;
        for (; i < P_S_MESSAGE_SIZE-1 && *aux_buffer_input != '\n' && *aux_buffer_input != '\0'; i++) {
            buffer_input[i] = *aux_buffer_input++;
        }
        for (; i < P_S_MESSAGE_SIZE; i++) {
            buffer_input[i] = '\0';
        }
        bytes_written = write(named_pipe, buffer_input, sizeof(char)*P_S_MESSAGE_SIZE);
        if (bytes_written < 0) {
            fputs(buffer, stdout);
            fprintf(stdout, "ERROR %s\n", "Failed to write pipe");
            return EXIT_FAILURE;
        }

        
    }

    if (close(named_pipe) < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to close pipe");
        return EXIT_FAILURE;
    }
    fprintf(stdout, "%s\n", "LOGOUT");
    if (unlink(pipe_name) != 0 && errno != ENOENT) {
        fprintf(stdout, "ERROR unlink(%s) failed:\n", pipe_name);
        return EXIT_FAILURE;
    }

    return 0;
}
