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
    buffer[i++] = '2';
    buffer[i++] = '|';
    for (; i < PIPE_NAME_SIZE+1 && *aux_pipe_name != '\0'; i++) {
        buffer[i] = *aux_pipe_name++;
    }
    for (; i < PIPE_NAME_SIZE+2; i++) {
        buffer[i] = '\0';
    }
    buffer[i++] = '|';
    for (; i < PIPE_PLUS_BOX_SIZE+2 && *aux_box_name != '\0'; i++) {
        buffer[i] = *aux_box_name++;
    }
    for (; i < PIPE_PLUS_BOX_SIZE+3; i++) {
        buffer[i] = '\0';
    }
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stdout, "ERROR %s\n", "subscriber: need more arguments\n");
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
    
    ssize_t bytes_written = write(register_pipe, buffer, sizeof(char)*(PIPE_PLUS_BOX_SIZE+3));
    if (bytes_written < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to write pipe");
        return EXIT_FAILURE;
    }
    if (close(register_pipe) < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to close pipe");
        return EXIT_FAILURE;
    }

    
    char aux_buffer[3];
    int aux_pipe = open(pipe_name, O_RDONLY);
    if (aux_pipe < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to open pipe");
        return EXIT_FAILURE;
    }
    if (read(aux_pipe, aux_buffer, sizeof(char)*3) < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to read pipe");
        return EXIT_FAILURE;
    }
    if (close(aux_pipe) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to close pipe");
        return EXIT_FAILURE;
    }
    if (strcmp(aux_buffer, "OK")) {
        printf("NIGGA\n");
        unlink(pipe_name);
        return -1;
    }

    if ((named_pipe = open(pipe_name, O_RDONLY)) < 0) {
        fprintf(stdout, "%s\n", pipe_name);
        fprintf(stdout, "ERROR %s\n", "Failed to open pipe");
        unlink(pipe_name);
        return EXIT_FAILURE;
    }

    signal(SIGINT, sig_handler);

    char message[P_S_MESSAGE_SIZE+3];
    char message_buffer[P_S_MESSAGE_SIZE];
    while (true) {
        ssize_t bytes_read = read(named_pipe, message, sizeof(char)*(P_S_MESSAGE_SIZE+3));
        if (bytes_read > 0) {
            for (int i = 0; i < P_S_MESSAGE_SIZE; i++) {
                message_buffer[i] = message[i+3];
            }
            fprintf(stdout, "%s\n", message_buffer);
        } else if (bytes_read == 0) {}
        
        else {
            fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
            unlink(pipe_name);
            return EXIT_FAILURE;
        }
    }

    return 0;
}
