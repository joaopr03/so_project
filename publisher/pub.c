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

static void sig_handler(int sig) {
    if (sig == EOF) {
        fprintf(stderr, "Caught EOF\n");
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

    signal(EOF, sig_handler); //iplement in process client instead of here
    
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
    
    ssize_t bytes_written = write(register_pipe, buffer, sizeof(char)*(PIPE_NAME_SIZE+2));
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