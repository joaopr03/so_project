#include "logging.h"
#include "config.h"
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>

static char *register_pipe_name;
static char *pipe_name;
static char *box_name;
static int named_pipe;

static void sig_handler(int sig) {
    if (sig == SIGINT) {
        if (close(named_pipe) < 0) {
            fprintf(stdout, "ERROR %s\n", "Failed to close pipe");
            unlink(pipe_name);
        }
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
    buffer[i++] = '2';
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
    if (argc < 4) {
        // || strcmp(argv[0], "sub")) {
        fprintf(stdout, "ERROR %s ; argv[0] = %s\n", "subscriber: need more arguments\n", argv[0]);
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

    char message[P_S_MESSAGE_SIZE];
        if ((named_pipe = open(pipe_name, O_RDONLY)) < 0) {
            fprintf(stdout, "%s\n", pipe_name);
            fprintf(stdout, "ERROR %s\n", "Failed to open pipe");
            unlink(pipe_name);
            return EXIT_FAILURE;
        }

    while (true) {

        signal(SIGINT, sig_handler);

        ssize_t bytes_read = read(named_pipe, message, sizeof(char)*P_S_MESSAGE_SIZE);
        if (bytes_read > 0) {
            fprintf(stdout, "%s\n", message);
        } else if (bytes_read == 0) {}
        
        else {
            fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
            unlink(pipe_name);
            return EXIT_FAILURE;
        }

        //fputs(buffer, stdout);
        /* for (int i = 0; i < 1028; i++) {
            putchar(buffer[i]);
        } */
        
    }

    /* if (close(named_pipe) < 0) {
            fprintf(stdout, "ERROR %s\n", "Failed to close pipe");
            unlink(pipe_name);
            return EXIT_FAILURE;
        }
    if (unlink(pipe_name) != 0 && errno != ENOENT) {
        fprintf(stdout, "ERROR unlink(%s) failed:\n", pipe_name);
        return EXIT_FAILURE;
    } */

    return 0;
}
