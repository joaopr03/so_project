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
    OP_CODE_PUB_MSG = '9'
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
static char **pipe_names;

int createFIFO(const char *named_pipe) {
    if (!strcmp(register_pipe_name, named_pipe))
        return -1;
    for (int i = 0; i < max_sessions; i++) {
        if (pipe_names[i] != NULL && !strcmp(pipe_names[i], named_pipe))
            return -1;
    }
    if (unlink(named_pipe) != 0 && errno != ENOENT) {
        fprintf(stdout, "ERROR unlink(%s) failed:\n", named_pipe);
        return -1;
    }
    if (mkfifo(named_pipe, 0666) != 0) {
        fprintf(stdout, "ERROR %s\n", "mkfifo failed\n");
        return -1;
    }
    for (int i = 0; i < max_sessions; i++) {
        if (pipe_names[i] == NULL) {
            pipe_names[i] = malloc(sizeof(char)*256);
            strcpy(pipe_names[i], named_pipe);
            return 0;
        }
    }
    return -1;

}

void destroy_server(int status) {
    free(workers);
    for (int i = 0; i < max_sessions; i++) {
        if (pipe_names[i] != NULL && unlink(pipe_names[i]) != 0 && errno != ENOENT) {
            fprintf(stdout, "ERROR %s\n", "Failed to unlink pipes");
            exit(EXIT_FAILURE);
        }
    }
    free(pipe_names);

    close(register_pipe);
    if (unlink(register_pipe_name) != 0 && errno != ENOENT) {
        fprintf(stdout, "ERROR %s\n", "Failed to delete pipe");
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
    pipe_names = malloc((unsigned int) max_sessions);
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
        pipe_names[i] = NULL;
    }
    return 0;
}

int process_entry(char *client, char *box) {
    char aux;
    ssize_t bytes_read = read(register_pipe, &aux, sizeof(char)); //read the "|"
    bytes_read = read(register_pipe, client, 256 * sizeof(char));
    if (bytes_read < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to read pipe");
        destroy_server(EXIT_FAILURE);
        return -1;
    }
    client[255] = '\0';
    bytes_read = read(register_pipe, box, 32 * sizeof(char));
    if (bytes_read < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to read pipe");
        destroy_server(EXIT_FAILURE);
        return -1;
    }
    box[31] = '\0';
    return 0;
}

int register_entry() {
    char client_named_pipe_path[256];
    char box_name[32];
    int fhandle;
    
    if (process_entry(client_named_pipe_path, box_name) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to read pipe");
        return -1;
    }
    if ((fhandle = tfs_open(box_name, TFS_O_APPEND)) != 0) {
        fprintf(stdout, "ERROR %s\n", "Box does not exist");
        return -1;
    }
    if (tfs_close(fhandle) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed close file");
        return -1;
    }
    if (createFIFO(client_named_pipe_path) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed create pipe");
        return -1;
    }
    fprintf(stdout, "Sucessfully created\n");
    return 0;
}

int create_box() {
    char client_named_pipe_path[256];
    char box_name[32];
    int fhandle;
    
    if (process_entry(client_named_pipe_path, box_name) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to read pipe");
        return -1;
    }
    if ((fhandle = tfs_open(box_name, TFS_O_CREAT)) != 0) {
        fprintf(stdout, "ERROR %s: %s\n%s\n", "Invalid box name", box_name, client_named_pipe_path);
        return -1;
    }
    if (tfs_close(fhandle) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed close file");
        return -1;
    }
    if (createFIFO(client_named_pipe_path) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed create pipe");
        return -1;
    }


    if (unlink(client_named_pipe_path) != 0 && errno != ENOENT) {
        fprintf(stdout, "ERROR unlink(%s) failed:\n", client_named_pipe_path);
        return -1;
    }
    for (int i = 0; i < max_sessions; i++) {
        if (pipe_names[i] != NULL && !strcmp(pipe_names[i], client_named_pipe_path)) {
            free(pipe_names[i]);
            pipe_names[i] = NULL;
        }
    }
    fprintf(stdout, "Box sucessfully created\n");
    return 0;
}
int remove_box() {
    char client_named_pipe_path[256];
    char box_name[32];
    int fhandle;
    
    if (process_entry(client_named_pipe_path, box_name) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to read pipe");
        return -1;
    }
    if ((fhandle = tfs_open(box_name, TFS_O_APPEND)) != 0) {
        fprintf(stdout, "ERROR %s\n", "Box does not exist");
        return -1;
    }
    if (tfs_close(fhandle) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed close file");
        return -1;
    }
    if (tfs_unlink(box_name) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed unlink file");
        return -1;
    }
    if (createFIFO(client_named_pipe_path) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed create pipe");
        return -1;
    }


    if (unlink(client_named_pipe_path) != 0 && errno != ENOENT) {
        fprintf(stdout, "ERROR unlink(%s) failed:\n", client_named_pipe_path);
        return -1;
    }
    for (int i = 0; i < max_sessions; i++) {
        if (pipe_names[i] != NULL && !strcmp(pipe_names[i], client_named_pipe_path)) {
            free(pipe_names[i]);
            pipe_names[i] = NULL;
        }
    }
    fprintf(stdout, "Box sucessfully removed\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {// || strcmp(argv[0], "mbroker")) {
        fprintf(stdout, "ERROR %s ; argv[0] = %s\n", "mbroker: need more arguments\n", argv[0]);
        return EXIT_SUCCESS;
    }
    register_pipe_name = argv[1];
    sscanf(argv[2], "%d", &max_sessions);

    if (init_server() != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to init server\n");
        return EXIT_FAILURE;
    }

    if (tfs_init(NULL) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to init tfs\n");
        return EXIT_FAILURE;
    }

    if (unlink(register_pipe_name) != 0 && errno != ENOENT) {
        fprintf(stdout, "ERROR unlink(%s) failed:\n", register_pipe_name);
        return EXIT_FAILURE;
    }

    // Create pipe
    if (mkfifo(register_pipe_name, 0666) != 0) {
        fprintf(stdout, "ERROR %s\n", "mkfifo failed\n");
        return EXIT_FAILURE;
    }

    /* // Open pipe read
    register_pipe = open(register_pipe_name, O_RDONLY);
    if (register_pipe < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to open server pipe");
        unlink(register_pipe_name);
        exit(EXIT_FAILURE);
    } */

    while(true) {
        signal(SIGINT, sig_handler); //is this supposed to be here

        register_pipe = open(register_pipe_name, O_RDONLY);
        if (register_pipe < 0) {
            fprintf(stdout, "ERROR %s\n", "Failed to open server pipe");
            unlink(register_pipe_name);
            destroy_server(EXIT_FAILURE);
        }

        ssize_t bytes_read;
        uint8_t op_code;
        do {
            bytes_read = read(register_pipe, &op_code, sizeof(char));
        } while (bytes_read < 0 && errno == EINTR);

        // main listener loop
        while (bytes_read > 0) {
            switch (op_code) {
            case OP_CODE_PUBLISHER:
                printf("PUBLISHER: ");
                register_entry();
                break;
            case OP_CODE_SUBSCRIBER:
                printf("SUBSCRIBER: ");
                register_entry();
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
    
    destroy_server(EXIT_SUCCESS);
    return 0;
}
/* (void)argc;
    (void)argv;
    fprintf(stderr, "usage: mbroker <pipename>\n");
    WARN("unimplemented"); // TODO: implement
    return -1; */