#include "logging.h"
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

static void sig_handler(int sig) {
    if (sig == SIGINT) {
        fprintf(stderr, "Caught SIGINT\n");
        exit(EXIT_SUCCESS);
    }
}

void create_register(char *buffer) {
    int i = 0;
    buffer[i++] = '2';
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

int main(int argc, char **argv) {
    if (argc < 4) {// || strcmp(argv[0], "sub")) {
        fprintf(stdout, "ERROR %s ; argv[0] = %s\n", "subscriber: need more arguments\n", argv[0]);
        return -1;
    }

    signal(SIGINT, sig_handler); //iplement in process client instead of here
    
    register_pipe_name = argv[1];
    pipe_name = argv[2];
    box_name = argv[3];

    char buffer[291];
    create_register(buffer);

    int register_pipe = open(register_pipe_name, O_WRONLY);
    if (register_pipe < 0) {
        if (errno == ENOENT)
            return 0;
        fprintf(stdout, "ERROR %s\n", "Failed to open server pipe");
        return EXIT_FAILURE;
    }
    
    ssize_t bytes_written = write(register_pipe, buffer, sizeof(char)*290);
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
    fprintf(stderr, "usage: sub <register_pipe_name> <box_name>\n");
    WARN("unimplemented"); // TODO: implement
    return -1; */