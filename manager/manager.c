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
static char mode;

enum {
    CODE_BOX_CREATE = '3',
    CODE_BOX_REMOVE = '5',
    CODE_BOX_LIST = '7'
};

void create_register(char *buffer) {
    int i = 0;
    char *aux_pipe_name = pipe_name;
    char *aux_box_name = box_name;
    buffer[i++] = mode;
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

int cmpBoxes(int a, int b) {
  return (strcmp(box[a].name, box[b].name) > 0);
}

void bubbleSort(int indexes[], int size, int (*cmpFunc) (int a, int b)) {
  int i, j, done;
  
  for (i = 0; i < size-1; i++){
    done=1;
    for (j = size-1; j > i; j--) 
    	if ((*cmpFunc)(indexes[j-1], indexes[j])) {
			int aux = indexes[j];
			indexes[j] = indexes[j-1];
			indexes[j-1] = aux;
			done=0;
    	}
    if (done) break;
  }
}

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stdout, "ERROR %s\n", "manager: need more arguments\n");
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

    int named_pipe;
    if ((named_pipe = open(pipe_name, O_RDONLY)) < 0) {
        fprintf(stdout, "%s\n", pipe_name);
        fprintf(stdout, "ERROR %s\n", "Failed to open pipe");
        unlink(pipe_name);
        return EXIT_FAILURE;
    }

    free(buffer);
    buffer = (char*) malloc(sizeof(char)*(ERROR_MESSAGE_SIZE+5));
    while (true) {
        ssize_t bytes_read = read(named_pipe, buffer, sizeof(char)*(ERROR_MESSAGE_SIZE+5));
        if (bytes_read == 0) {
            char error_message[ERROR_MESSAGE_SIZE];
            switch (buffer[2]) {
            case '0':
                fprintf(stdout, "OK\n");
                break;
            default:
                for (int i = 5; i < ERROR_MESSAGE_SIZE+5 && buffer[i] != '\0'; i++) {
                    error_message[i-5] = buffer[i];
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

    return 0;
}
