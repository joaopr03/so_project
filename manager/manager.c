#include "logging.h"
#include "config.h"
#include <stdint.h>
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
static int named_pipe;

typedef struct {
    char name[BOX_NAME_SIZE];
    uint64_t size;
    uint64_t n_publishers;
    uint64_t n_subscribers;
} box_t;

int n_boxes = 0;

enum {
    CODE_BOX_CREATE = '3',
    CODE_BOX_CREATE_R = '4',
    CODE_BOX_REMOVE = '5',
    CODE_BOX_REMOVE_R = '6',
    CODE_BOX_LIST = '7',
    CODE_BOX_LIST_R = '8'
};

void create_register(char *buffer) {
    int i = 0;
    char *aux_pipe_name = pipe_name;
    char *aux_box_name = box_name;
    buffer[i++] = mode;
    buffer[i++] = '|';
    for (; i < PIPE_NAME_SIZE+1 && *aux_pipe_name != '\0'; i++) {
        buffer[i] = *aux_pipe_name++;
    }
    for (; i < PIPE_NAME_SIZE+2; i++) {
        buffer[i] = '\0';
    }
    switch (mode) {
    case CODE_BOX_CREATE: case CODE_BOX_REMOVE:
        buffer[i++] = '|';
        for (; i < PIPE_PLUS_BOX_SIZE+2 && *aux_box_name != '\0'; i++) {
            buffer[i] = *aux_box_name++;
        }
        for (; i < PIPE_PLUS_BOX_SIZE+3; i++) {
            buffer[i] = '\0';
        }
    default:
        break;
    }
}

/* int cmpBoxes(int a, int b) {
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
} */

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
    case CODE_BOX_LIST:
        buffer = (char*) malloc(sizeof(char) *(PIPE_NAME_SIZE+2));
        create_register(buffer);
        bytes_written = write(register_pipe, buffer, sizeof(char)*(PIPE_NAME_SIZE+1));
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

        if ((named_pipe = open(pipe_name, O_RDONLY)) < 0) {
            fprintf(stdout, "%s\n", pipe_name);
            fprintf(stdout, "ERROR %s\n", "Failed to open pipe");
            unlink(pipe_name);
            return EXIT_FAILURE;
        }

        free(buffer);
        box_t *boxes = malloc(sizeof(box_t)*5);
        buffer = (char*) malloc(sizeof(char)*(BOX_NAME_SIZE+17));
        int box_index = 0;
        printf("%d\n", n_boxes);
        while (true) {
            ssize_t bytes_read = read(named_pipe, buffer, sizeof(char)*(BOX_NAME_SIZE+17));
            if (bytes_read > 0) {
                if (buffer[2] == '1' && buffer[4] == '\0') {
                    fprintf(stdout, "NO BOXES FOUND\n");
                    break;
                }
                n_boxes++;
                if (n_boxes%5 == 0) {
                    boxes = realloc(boxes, sizeof(box_t)*(unsigned int)(n_boxes+5));
                }
                for (int i = 4; i < BOX_NAME_SIZE+4; i++) {
                    boxes[box_index].name[i-4] = buffer[i];
                }
                /* printf("%s\n", buffer);
                printf("%s\n", boxes[box_index].name); */
                char aux[5];
                for (int i = 4; i < 9; i++) {
                    aux[i-4] = buffer[BOX_NAME_SIZE+i];
                }
                aux[4] = '\0';
                printf("%s\n", aux);
                sscanf(aux, "%zu", &boxes[box_index].size);
                printf("%zu\n", boxes[box_index].size);
                boxes[box_index].n_publishers = (uint64_t) (buffer[BOX_NAME_SIZE+10] - '0');
                for (int i = 12; i < 16; i++) {
                    aux[i-12] = buffer[BOX_NAME_SIZE+i];
                }
                aux[4] = '\0';
                sscanf(aux, "%zu", &boxes[box_index].n_subscribers);
                box_index++;

            } else if (bytes_read < 0) {
                fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
                free(buffer);
                unlink(pipe_name);
                return EXIT_FAILURE;
            } else {
                /* if (n_boxes == 0) {
                    fprintf(stdout, "NO BOXES FOUND\n");
                    break;
                } */
                for(int i = 0; i < n_boxes; i++) {
                    fprintf(stdout, "%s %zu %zu %zu\n", boxes[i].name, boxes[i].size, boxes[i].n_publishers, boxes[i].n_subscribers);
                }
                break;
            }
        }
        free(buffer);
        if (n_boxes != 0) {
            free(boxes);
        }
        unlink(pipe_name);
        return 0;
    default:
        unlink(pipe_name);
        return EXIT_FAILURE;
    }
    return 0;
}
