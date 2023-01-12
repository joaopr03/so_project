#include "logging.h"
#include "operations.h"
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>

enum {
    OP_CODE_PUBLISHER = '1',
    OP_CODE_SUBSCRIBER = '2',
    OP_CODE_BOX_CREATE = '3',
    OP_CODE_BOX_CREATE_R = '4',
    OP_CODE_BOX_REMOVE = '5',
    OP_CODE_BOX_REMOVE_R = '6',
    OP_CODE_BOX_LIST = '7',
    OP_CODE_BOX_LIST_R = '8',
    OP_CODE_PUB_MSG = '9',
    OP_CODE_SUB_MSG = '0'
};

pthread_t threads;
typedef struct {
    int session_id;
    bool to_execute;
    pthread_t tid;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} worker_t;

worker_t *workers;
static int register_pipe;
static char *register_pipe_name;
static int max_sessions;

void destroy_server(int status) {
    free(workers);
    close(register_pipe);
    if (unlink(register_pipe_name) != 0 && errno != ENOENT) {
        fprintf(stdout, "ERROR %s\n", "Failed to delete pipe");
        exit(EXIT_FAILURE);
    }
    if (tfs_destroy() != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to destroy tfs\n");
        exit(EXIT_FAILURE);
    }
    printf("\nSuccessfully ended the server.\n");
    exit(status);
}

static void sig_handler(int sig) {
    if (sig == SIGINT) {
        destroy_server(EXIT_SUCCESS);
    }
}

void *session_worker(void *args) {
    worker_t *worker = (worker_t*)args;
    while (true) {
        if (pthread_mutex_lock(&worker->lock) != 0) {
            exit(EXIT_FAILURE);
        }

        while (!worker->to_execute) {
            if (pthread_cond_wait(&worker->cond, &worker->lock) != 0) {
                fprintf(stdout, "ERROR %s\n", "Failed to wait for condition variable");
                destroy_server(EXIT_FAILURE);
            }
        }
        worker->to_execute = false;
        if (pthread_mutex_unlock(&worker->lock) != 0) {
            exit(EXIT_FAILURE);
        }
    }
}

int init_server() {
    workers = malloc(sizeof(worker_t)*(unsigned int) max_sessions);
    for (int i = 0; i < max_sessions; ++i) {
        workers[i].session_id = i;
        workers[i].to_execute = false;
        if (pthread_mutex_init(&workers[i].lock, NULL) != 0) {
            return -1;
        }
        if (pthread_cond_init(&workers[i].cond, NULL) != 0) {
            return -1;
        }
        if (pthread_create(&workers[i].tid, NULL, session_worker, &workers[i]) != 0) {
            return -1;
        }
    }
    if (tfs_init(NULL) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to init tfs\n");
        return EXIT_FAILURE;
    }
    return 0;
}

int process_entry(char *client, char *box) {
    char aux;
    ssize_t bytes_read = read(register_pipe, &aux, sizeof(char)); //read the "|"
    bytes_read = read(register_pipe, client, PIPE_NAME_SIZE*sizeof(char));
    if (bytes_read < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to read pipe");
        destroy_server(EXIT_FAILURE);
        return -1;
    }
    client[PIPE_NAME_SIZE-1] = '\0';
    bytes_read = read(register_pipe, box, BOX_NAME_SIZE*sizeof(char));
    if (bytes_read < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to read pipe");
        destroy_server(EXIT_FAILURE);
        return -1;
    }
    box[BOX_NAME_SIZE-1] = '\0';
    return 0;
}

void box_feedback(char *buffer, char box_operation, int32_t return_code, char *error_message) {
    int i = 0;
    buffer[i++] = box_operation;
    buffer[i++] = '|';
    switch (return_code) {
    case EXIT_SUCCESS:
        buffer[i++] = '0';
        break;
    default:
        buffer[i++] = '1';
        break;
    }
    buffer[i++] = '|';
    
    if (return_code != 0) {
        for (; i < ERROR_MESSAGE_SIZE+2 && *error_message != '\0'; i++) {
            buffer[i] = *error_message++;
        }
    }
    for (; i < ERROR_MESSAGE_SIZE+3; i++) {
        buffer[i] = '\0';
    }
}

int register_entry(char *client_named_pipe_path, char *box_name) {
    int fhandle;
    if (process_entry(client_named_pipe_path, box_name) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to read pipe");
        return -1;
    }
    if ((fhandle = tfs_open(box_name, TFS_O_APPEND)) < 0) {
        fprintf(stdout, "ERROR %s\n", "Box does not exist");
        return -1;
    }
    if (tfs_close(fhandle) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed close file");
        return -1;
    }
    return 0;
}

int start_publisher() {
    char client_named_pipe_path[PIPE_NAME_SIZE];
    char box_name[BOX_NAME_SIZE];
    if (register_entry(client_named_pipe_path, box_name) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to create publisher");
        return -1;
    }
    fprintf(stdout, "Sucessfully created\n");
    char buffer[P_S_MESSAGE_SIZE];
    int named_pipe;
    if ((named_pipe = open(client_named_pipe_path, O_RDONLY)) < 0) {
        fprintf(stdout, "%s\n", client_named_pipe_path);
        fprintf(stdout, "ERROR %s\n", "Failed to open pipe");
        return EXIT_FAILURE;
    }
    while (true) {
        ssize_t bytes_read = read(named_pipe, buffer, sizeof(char)*P_S_MESSAGE_SIZE);
        if (bytes_read > 0) {
            buffer[P_S_MESSAGE_SIZE-1] = '\0';
            int fhandle = tfs_open(box_name, TFS_O_APPEND);
            
            if (tfs_write(fhandle, buffer, (size_t) bytes_read) < 0) {
                fprintf(stdout, "ERROR %s\n", "Failed to write box");
                tfs_close(fhandle);
                return EXIT_FAILURE;
            }
            printf("%s\n", buffer);
            if (tfs_close(fhandle) != 0) {
                fprintf(stdout, "ERROR %s\n", "Failed to close file");
                return EXIT_FAILURE;
            }
        }
        else if (bytes_read == 0) {
            int fhandle = tfs_open(box_name, 0);
            char buffer1[P_S_MESSAGE_SIZE];
            while (tfs_read(fhandle, buffer1, P_S_MESSAGE_SIZE) != 0) {
                printf("%s\n", buffer1);
            }
            tfs_close(fhandle);
            return 0;
        }
        else {
            fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
    }
    return 0;
}

int start_subscriber() {
    char client_named_pipe_path[PIPE_NAME_SIZE];
    char box_name[BOX_NAME_SIZE];
    if (register_entry(client_named_pipe_path, box_name) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to create subscriber");
        return -1;
    }
    fprintf(stdout, "Sucessfully created\n");
    char buffer[P_S_MESSAGE_SIZE];
    int named_pipe;
    if ((named_pipe = open(client_named_pipe_path, O_WRONLY)) < 0) {
        fprintf(stdout, "%s\n", client_named_pipe_path);
        fprintf(stdout, "ERROR %s\n", "Failed to open pipe");
        return EXIT_FAILURE;
    }
        
    ssize_t bytes_written;
    ssize_t bytes_read;
    int fhandle = tfs_open(box_name, 0);
    while (true) {
        bytes_read = tfs_read(fhandle, buffer, P_S_MESSAGE_SIZE*sizeof(char));
        if (bytes_read == 0) {
            break;
        } else if (bytes_read < 0) {
            fprintf(stdout, "ERROR %s\n", "Failed to read box");
            tfs_close(fhandle);
            return EXIT_FAILURE;
        }

        /* buffer[P_S_MESSAGE_SIZE-1] = '\0'; */
        printf("%s\n", buffer);

        bytes_written = write(named_pipe, buffer, sizeof(char)*P_S_MESSAGE_SIZE);
        if (bytes_written < 0) {
            fprintf(stdout, "ERROR %s\n", "Failed to write pipe");
            return EXIT_FAILURE;
        }
    }
    if (tfs_close(fhandle) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to close file");
        return EXIT_FAILURE;
    }  
    if (close(named_pipe) < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to close pipe");
        return EXIT_FAILURE;
    }
    return 0;
}

int send_message_manager(const char *pipe_path, char operation, int32_t code, char *message) {
    int named_pipe = open(pipe_path, O_WRONLY);
    if (named_pipe < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to open pipe");
        return -1;
    }

    char buffer[ERROR_MESSAGE_SIZE+4];
    box_feedback(buffer, operation, code, message);
    ssize_t bytes_written = write(named_pipe, buffer, sizeof(char)*(ERROR_MESSAGE_SIZE+4));
    if (bytes_written < 0) {
        fputs(buffer, stdout);
        fprintf(stdout, "ERROR %s\n", "Failed to write pipe");
        return -1;
    }
    if (close(named_pipe) < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to close pipe");
        return -1;
    }
    return 0;
}

int create_box() {
    char client_named_pipe_path[PIPE_NAME_SIZE];
    char box_name[BOX_NAME_SIZE];
    int fhandle;
    
    if (process_entry(client_named_pipe_path, box_name) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to read pipe");
        return -1;
    }
    if ((fhandle = tfs_open(box_name, TFS_O_APPEND)) >= 0) {
        if (tfs_close(fhandle) != 0)
            send_message_manager(client_named_pipe_path, '6', -1,"create_box: Failed close file");
        send_message_manager(client_named_pipe_path, '6', -1,"create_box: Box already exist");
        return -1;
    }
    if ((fhandle = tfs_open(box_name, TFS_O_CREAT)) < 0) {
        send_message_manager(client_named_pipe_path, '6', -1,"create_box: Failed create file");
        return -1;
    }
    if (tfs_close(fhandle) != 0) {
        send_message_manager(client_named_pipe_path, '6', -1,"create_box: Failed close file");
        return -1;
    }

    send_message_manager(client_named_pipe_path, '4', 0, "");
    fprintf(stdout, "OK\n");
    return 0;
}

int remove_box() {
    char client_named_pipe_path[PIPE_NAME_SIZE];
    char box_name[BOX_NAME_SIZE];
    int fhandle;
    
    if (process_entry(client_named_pipe_path, box_name) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to read pipe");
        return -1;
    }
    if ((fhandle = tfs_open(box_name, TFS_O_APPEND)) < 0) {
        send_message_manager(client_named_pipe_path, '6', -1,"remove_box: Box does not exist");
        return -1;
    }
    if (tfs_close(fhandle) != 0) {
        send_message_manager(client_named_pipe_path, '6', -1,"remove_box: Failed close file");
        return -1;
    }
    if (tfs_unlink(box_name) != 0) {
        send_message_manager(client_named_pipe_path, '6', -1,"remove_box: Failed unlink file");
        return -1;
    }

    send_message_manager(client_named_pipe_path, '6', 0, "");
    fprintf(stdout, "OK\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stdout, "ERROR %s\n", "mbroker: need more arguments\n");
        return EXIT_SUCCESS;
    }
    register_pipe_name = argv[1];
    sscanf(argv[2], "%d", &max_sessions);
    if (init_server() != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to init server\n");
        return EXIT_FAILURE;
    }

    if (unlink(register_pipe_name) != 0 && errno != ENOENT) {
        fprintf(stdout, "ERROR unlink(%s) failed:\n", register_pipe_name);
        return EXIT_FAILURE;
    }
    if (mkfifo(register_pipe_name, 0666) != 0) {
        fprintf(stdout, "ERROR %s\n", "mkfifo failed\n");
        return EXIT_FAILURE;
    }

    signal(SIGINT, sig_handler);

    ssize_t bytes_read;
    uint8_t op_code;
    while(true) {
        register_pipe = open(register_pipe_name, O_RDONLY);
        if (register_pipe < 0) {
            fprintf(stdout, "ERROR %s\n", "Failed to open server pipe");
            unlink(register_pipe_name);
            destroy_server(EXIT_FAILURE);
        }

        do {
            bytes_read = read(register_pipe, &op_code, sizeof(char));
        } while (bytes_read < 0 && errno == EINTR);

        while (bytes_read > 0) {
            switch (op_code) {
            case OP_CODE_PUBLISHER:
                printf("PUBLISHER: ");
                start_publisher();
                break;
            case OP_CODE_SUBSCRIBER:
                printf("SUBSCRIBER: ");
                start_subscriber();
                break;
            case OP_CODE_BOX_CREATE:
                printf("BOX_CREATE: ");
                create_box();
                break;
            case OP_CODE_BOX_REMOVE:
                printf("BOX_REMOVE: ");
                remove_box();
                break;
            case OP_CODE_BOX_LIST:
                printf("BOX_LIST: ");
                break;
            case OP_CODE_PUB_MSG:
                printf("MESSAGE_FROM_PUBLISHER: ");
                break;
            default:
                printf("SWITCH CASE DOES NOT WORK\n");
                break;
            }

            do {
                bytes_read = read(register_pipe, &op_code, sizeof(char));
            } while (bytes_read < 0 && errno == EINTR);
        }
        if (bytes_read < 0) {
            fprintf(stdout, "ERROR %s\n", "Failed to read pipe");
            destroy_server(EXIT_FAILURE);
        }
        
        if (close(register_pipe) < 0) {
            perror("Failed to close pipe");
            destroy_server(EXIT_FAILURE);
        }
    }

    return 0;
}
