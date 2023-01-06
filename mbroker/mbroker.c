#include "logging.h"
#include "operations.h"
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

enum {
    CODE_PUBLISHER = 1,
    CODE_SUBSCRIBER = 2,
    CODE_BOX_CREATE = 3,
    CODE_BOX_CREATE_R = 4,
    CODE_BOX_REMOVE = 5,
    CODE_BOX_REMOVE_R = 6,
    CODE_BOX_LIST = 7,
    CODE_BOX_LIST_R = 8,
    CODE_PUB_MSG = 9
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
    if (close(register_pipe) < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to close pipe");
        exit(EXIT_FAILURE);
    }

    if (unlink(register_pipe_name) != 0 && errno != ENOENT) {
        fprintf(stdout, "ERROR %s\n", "Failed to delete pipe");
        exit(EXIT_FAILURE);
    }

    printf("\nSuccessfully ended the server.\n");
    exit(status);
}

void *session_worker(void *args) {
    worker_t *worker = (worker_t *)args;
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
    workers = malloc(sizeof(pthread_t) * (unsigned int) max_sessions);
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
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2 || argv[0] != "mbroker") {
        fprintf(stdout, "ERROR %s ; argv[0] = \n", "mbroker: need more arguments\n", argv[0]);
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
    if (mkfifo(register_pipe_name, 0777) != 0) {
        fprintf(stdout, "ERROR %s\n", "mkfifo failed\n");
        return EXIT_FAILURE;
    }

    // Open pipe read
    register_pipe = open(register_pipe_name, O_RDONLY);
    if (register_pipe < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to open server pipe");
        unlink(register_pipe_name);
        exit(EXIT_FAILURE);
    }

    while(true) {
        int tmp = open(register_pipe_name, O_RDONLY);
        if (tmp < 0) {
            if (errno == ENOENT)
                return 0;
            fprintf(stdout, "ERROR %s\n", "Failed to open server pipe");
            destroy_server(EXIT_FAILURE);
        }
        if (close(tmp) < 0) {
            perror("Failed to close pipe");
            destroy_server(EXIT_FAILURE);
        }

        ssize_t bytes_read;
        char op_code;
        do {
            bytes_read = read(register_pipe, &op_code, sizeof(char));
        } while (bytes_read < 0 && errno == EINTR);

        // main listener loop
        while (bytes_read > 0) {
            switch (op_code) {
            case CODE_PUBLISHER:
                break;
            case CODE_SUBSCRIBER:
                break;
            case CODE_BOX_CREATE:
                break;
            case CODE_BOX_CREATE_R:
                break;
            case CODE_BOX_REMOVE:
                break;
            case CODE_BOX_REMOVE_R:
                break;
            case CODE_BOX_LIST:
                    break;
            case CODE_BOX_LIST_R:
                break;
            case CODE_PUB_MSG:
                break;
            default:
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
    }
    destroy_server(EXIT_SUCCESS);
    return 0;
}
/* (void)argc;
    (void)argv;
    fprintf(stderr, "usage: mbroker <pipename>\n");
    WARN("unimplemented"); // TODO: implement
    return -1; */