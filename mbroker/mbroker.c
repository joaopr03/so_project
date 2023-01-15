#include "mbroker.h"

static int n_boxes = 0;
static worker_t *workers;
static bool *free_workers;
static pthread_mutex_t free_worker_lock;
box_t *boxes;
static int register_pipe;
static char *register_pipe_name;
static int max_sessions;
static pc_queue_t queue;
pthread_mutex_t register_pipe_lock;

void destroy_server(int status) {
    
    free(boxes);
    close(register_pipe);
    if (unlink(register_pipe_name) != 0 && errno != ENOENT) {
        fprintf(stdout, "ERROR %s\n", "Failed to delete pipe");
        exit(EXIT_FAILURE);
    }
    if (tfs_destroy() != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to destroy tfs\n");
        exit(EXIT_FAILURE);
    }
    if (pcq_destroy(&queue) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to destroy queue\n");
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_destroy(&free_worker_lock) != 0) {
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_destroy(&register_pipe_lock) != 0) {
        exit(EXIT_FAILURE);
    }
    free(queue.pcq_buffer);
    free(free_workers);
    free(workers);
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

        switch (worker->packet.opcode) {
        case OP_CODE_PUBLISHER:
            start_publisher();
            break;
        case OP_CODE_SUBSCRIBER:
            start_subscriber();
            break;
        case OP_CODE_BOX_CREATE:
            create_box();
            break;
        case OP_CODE_BOX_REMOVE:
            remove_box();
            break;
        case OP_CODE_BOX_LIST:
            list_boxes();
            break;
        default:
            break;
        }

        worker->to_execute = false;
        if (pthread_mutex_unlock(&worker->lock) != 0) {
            exit(EXIT_FAILURE);
        }
    }
}

int init_server() {
    workers = malloc(sizeof(worker_t)*(unsigned int) max_sessions);
    
    queue.pcq_buffer = malloc((unsigned int) max_sessions);
    if (pcq_create(&queue, (size_t) max_sessions) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to create queue\n");
        return EXIT_FAILURE;
    }
    free_workers = malloc(sizeof(bool)*(unsigned int) max_sessions);
    boxes = malloc(sizeof(box_t)*(unsigned int) max_sessions);
    for (int i = 0; i < max_sessions; i++) {
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
        free_workers[i] = false;

        boxes[i].n_subscribers = 0;
        boxes[i].n_publishers = 0;
        strcpy(boxes[i].name, "");
        boxes[i].size = 0;
    }
    if (pthread_mutex_init(&free_worker_lock, NULL) != 0) {
        return -1;
    }
    if (pthread_mutex_init(&register_pipe_lock, NULL) != 0) {
        return -1;
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
        return -1;
    }
    client[PIPE_NAME_SIZE-1] = '\0';

    if (box == NULL) {
        if (pthread_mutex_unlock(&register_pipe_lock) != 0) {
            return -1;
        }
        return 0;
    }
    bytes_read = read(register_pipe, &aux, sizeof(char)); //read the "|"

    bytes_read = read(register_pipe, box, BOX_NAME_SIZE*sizeof(char));
    if (bytes_read < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to read pipe");
        return -1;
    }
    box[BOX_NAME_SIZE-1] = '\0';
    if (pthread_mutex_unlock(&register_pipe_lock) != 0) {
        return -1;
    }
    return 0;
}

void create_message(char *new_buffer, char *message) {
    int i = 0;
    char *aux_message = message;
    new_buffer[i++] = '1';
    new_buffer[i++] = '0';
    new_buffer[i++] = '|';
    for (; i < P_S_MESSAGE_SIZE+2 && *aux_message != '\0'; i++) {
        new_buffer[i] = *aux_message++;
    }
    for (; i < P_S_MESSAGE_SIZE+3; i++) {
        new_buffer[i] = '\0';
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
    for (int i = 0; i < max_sessions; i++) {
        if (!strcmp(box_name,boxes[i].name)) {
            return i;
        }
    }
    return -1;
}

int start_publisher() {
    char client_named_pipe_path[PIPE_NAME_SIZE];
    char box_name[BOX_NAME_SIZE];
    int named_pipe;

    int box_index;
    if ((box_index = register_entry(client_named_pipe_path, box_name)) < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to create publisher");
        named_pipe = open(client_named_pipe_path, O_WRONLY);
        if (write(named_pipe, "ER", sizeof(char)*3) < 0) {};
        close(named_pipe);
        return -1;
    }
    if (boxes[box_index].n_publishers == 1) {
        fprintf(stdout, "ERROR %s\n", "Failed to create publisher: box already linked");
        named_pipe = open(client_named_pipe_path, O_WRONLY);
        if (write(named_pipe, "ER", sizeof(char)*3) < 0) {};
        close(named_pipe);
        return -1;
    }
    named_pipe = open(client_named_pipe_path, O_WRONLY);
    if (named_pipe < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to open pipe");
        return EXIT_FAILURE;
    }
    if (write(named_pipe, "OK", sizeof(char)*3) < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to write pipe");
        return EXIT_FAILURE;
    }
    if (close(named_pipe) < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to close pipe");
        return EXIT_FAILURE;
    }
    boxes[box_index].n_publishers++;
    fprintf(stdout, "Sucessfully created publisher\n");

    char message_buffer[P_S_MESSAGE_SIZE+3];
    if ((named_pipe = open(client_named_pipe_path, O_RDONLY)) < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to open pipe");
        return EXIT_FAILURE;
    }
    char buffer[P_S_MESSAGE_SIZE];
    while (true) {  
        ssize_t bytes_read = read(named_pipe, message_buffer, sizeof(char)*(P_S_MESSAGE_SIZE+3));
        if (bytes_read > 0) {
            int fhandle = tfs_open(box_name, TFS_O_APPEND);
            size_t stringlen = strlen(message_buffer)-2;
            for (int i = 0; i < stringlen; i++) {
                buffer[i] = message_buffer[i+3];
            }
            buffer[stringlen-1] = '\0';
            ssize_t bytes_written = tfs_write(fhandle, buffer, stringlen);
            if (bytes_written <= 0) {
                fprintf(stdout, "ERROR %s\n", "Failed to write box");
                tfs_close(fhandle);
                return EXIT_FAILURE;
            }

            boxes[box_index].size += (uint64_t) bytes_written;
            //printf("%s\n", buffer); //print para debug
            if (tfs_close(fhandle) != 0) {
                fprintf(stdout, "ERROR %s\n", "Failed to close file");
                return EXIT_FAILURE;
            }
        }
        else if (bytes_read == 0) {
            /* int fhandle = tfs_open(box_name, 0);
            while (tfs_read(fhandle, buffer, P_S_MESSAGE_SIZE) > 0) {//print...
                int count = 0;                                  
                for (int j = 0; j < P_S_MESSAGE_SIZE; j++) {
                    if (buffer[j] == '\0') {
                        count++;
                        if (count == 2) break;
                        putchar('\n');
                    } else { count = 0; putchar(buffer[j]);}    //...para debug
                } 
            }
            tfs_close(fhandle);*/
            boxes[box_index].n_publishers--;
            fprintf(stdout, "Sucessfully ended publisher\n");
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
    int named_pipe;
    size_t bytes_processed = 0;

    int box_index;
    if ((box_index = register_entry(client_named_pipe_path, box_name)) < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to create subscriber");
        named_pipe = open(client_named_pipe_path, O_WRONLY);
        if (write(named_pipe, "ER", sizeof(char)*3) < 0) {};
        close(named_pipe);
        return -1;
    }
    named_pipe = open(client_named_pipe_path, O_WRONLY);
    if (named_pipe < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to open pipe");
        return EXIT_FAILURE;
    }
    if (write(named_pipe, "OK", sizeof(char)*3) < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to write pipe");
        return EXIT_FAILURE;
    }
    if (close(named_pipe) < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to close pipe");
        return EXIT_FAILURE;
    }
    boxes[box_index].n_subscribers++;
    fprintf(stdout, "Sucessfully created subscriber\n");

    if ((named_pipe = open(client_named_pipe_path, O_WRONLY)) < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to open pipe");
        return EXIT_FAILURE;
    }

    ssize_t bytes_written;
    ssize_t bytes_read;
    char message_buffer[P_S_MESSAGE_SIZE+3];
    while (true) {
        char buffer[P_S_MESSAGE_SIZE];
        int i = 0;
        int count = 0;
        int fhandle = tfs_open(box_name, 0);
        while (true) {
            char aux_char;
            bytes_read = tfs_read(fhandle, &aux_char, sizeof(char));
            count++;
            if (bytes_read <= 0) break;
            if (bytes_processed <= count) {
                buffer[i++] = aux_char;
                if (aux_char == '\0' && bytes_processed < count) break;
            }
        }
        bytes_processed = (size_t) count*sizeof(char);
        
        if (bytes_read < 0) {
            fprintf(stdout, "ERROR %s\n", "Failed to read box");
            tfs_close(fhandle);
            return EXIT_FAILURE;
        }

        if (mkfifo(client_named_pipe_path, 0666) == 0) { //if fifo closed in subscriber
            unlink(client_named_pipe_path);
            break;
        }
        
        if (bytes_read != 0) {
            create_message(message_buffer, buffer);
            bytes_written = write(named_pipe, message_buffer, sizeof(char)*(P_S_MESSAGE_SIZE+3));
            if (bytes_written < 0) {
                fprintf(stdout, "ERROR %s\n", "Failed to write pipe");
                return EXIT_FAILURE;
            }
        }
        if (tfs_close(fhandle) != 0) {
            fprintf(stdout, "ERROR %s\n", "Failed to close file");
            return EXIT_FAILURE;
        } 
    }

    if (close(named_pipe) < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to close pipe");
        return EXIT_FAILURE;
    }
    boxes[box_index].n_subscribers--;
    fprintf(stdout, "Sucessfully ended subscribers\n");
    return 0;
}

int send_message_manager(const char *pipe_path, char operation, int32_t code, char *message) {
    int named_pipe = open(pipe_path, O_WRONLY);
    if (named_pipe < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to open pipe");
        return -1;
    }
    int i = (int) strlen(message);
    char error_message[ERROR_MESSAGE_SIZE];
    strcpy(error_message, message);
    for (; i < ERROR_MESSAGE_SIZE; i++) {
        error_message[i] = '\0';
    }

    ssize_t bytes_written = write(named_pipe, &operation, sizeof(char));
    bytes_written = write(named_pipe, "|", sizeof(char));
    bytes_written = write(named_pipe, &code, sizeof(int32_t));
    bytes_written = write(named_pipe, "|", sizeof(char));
    bytes_written = write(named_pipe, message, sizeof(char)*ERROR_MESSAGE_SIZE);

    if (bytes_written < 0) {
        fputs(error_message, stdout);
        fprintf(stdout, "ERROR %s\n", "Failed to write pipe");
        return -1;
    }
    if (close(named_pipe) < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to close pipe");
        return -1;
    }
    return 0;
}

int send_box_manager(const char *pipe_path, char last, box_t *box) {
    int named_pipe = open(pipe_path, O_WRONLY);
    if (named_pipe < 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to open pipe");
        return -1;
    }
    char buffer[4];
    int i = 0;
    buffer[i++] = OP_CODE_BOX_LIST_R;
    buffer[i++] = '|';
    buffer[i++] = last;
    buffer[i++] = '|';
    ssize_t bytes_written = write(named_pipe, buffer, sizeof(char)*4);

    if (box == NULL) {
        char box_name[BOX_NAME_SIZE];
        for (i = 0; i < BOX_NAME_SIZE; i++) {
            box_name[i] = '\0';
        }
        uint64_t aux = 0;
        bytes_written = write(named_pipe, box_name, sizeof(char)*BOX_NAME_SIZE);
        bytes_written = write(named_pipe, "|", sizeof(char));
        bytes_written = write(named_pipe, &aux, sizeof(uint64_t));
        bytes_written = write(named_pipe, "|", sizeof(char));
        bytes_written = write(named_pipe, &aux, sizeof(uint64_t));
        bytes_written = write(named_pipe, "|", sizeof(char));
        bytes_written = write(named_pipe, &aux, sizeof(uint64_t));
    } else {
        bytes_written = write(named_pipe, &box->name, sizeof(char)*BOX_NAME_SIZE);
        bytes_written = write(named_pipe, "|", sizeof(char));
        bytes_written = write(named_pipe, &box->size, sizeof(uint64_t));
        bytes_written = write(named_pipe, "|", sizeof(char));
        bytes_written = write(named_pipe, &box->n_publishers, sizeof(uint64_t));
        bytes_written = write(named_pipe, "|", sizeof(char));
        bytes_written = write(named_pipe, &box->n_subscribers, sizeof(uint64_t));
    }
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
            send_message_manager(client_named_pipe_path, OP_CODE_BOX_CREATE_R, -1,"create_box: Failed close file");
        send_message_manager(client_named_pipe_path, OP_CODE_BOX_CREATE_R, -1,"create_box: Box already exist");
        return -1;
    }
    if ((fhandle = tfs_open(box_name, TFS_O_CREAT)) < 0) {
        send_message_manager(client_named_pipe_path, OP_CODE_BOX_CREATE_R, -1,"create_box: Failed create file");
        return -1;
    }
    if (tfs_close(fhandle) != 0) {
        send_message_manager(client_named_pipe_path, OP_CODE_BOX_CREATE_R, -1,"create_box: Failed close file");
        return -1;
    }

    if (n_boxes == max_sessions) {
        send_message_manager(client_named_pipe_path, OP_CODE_BOX_CREATE_R, -1,"create_box: No space left");
        return -1;
    }

    for (int i = 0; i < max_sessions; i++) {
        if (!strcmp("",boxes[i].name)) {
            n_boxes++;
            strcpy(boxes[i].name, box_name);
            break;
        }
    }
    send_message_manager(client_named_pipe_path, OP_CODE_BOX_CREATE_R, 0, "");
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
        send_message_manager(client_named_pipe_path, OP_CODE_BOX_REMOVE_R, -1,"remove_box: Box does not exist");
        return -1;
    }
    if (tfs_close(fhandle) != 0) {
        send_message_manager(client_named_pipe_path, OP_CODE_BOX_REMOVE_R, -1,"remove_box: Failed close file");
        return -1;
    }
    if (tfs_unlink(box_name) != 0) {
        send_message_manager(client_named_pipe_path, OP_CODE_BOX_REMOVE_R, -1,"remove_box: Failed unlink file");
        return -1;
    }

    for (int i = 0; i < max_sessions; i++) {
        if (!strcmp(box_name, boxes[i].name)) {
            n_boxes--;
            strcpy(boxes[i].name, "");
            break;
        }
    }
    send_message_manager(client_named_pipe_path, OP_CODE_BOX_REMOVE_R, 0, "");
    fprintf(stdout, "OK\n");
    return 0;
}

int list_boxes() {
    char client_named_pipe_path[PIPE_NAME_SIZE];
    
    if (process_entry(client_named_pipe_path, NULL) != 0) {
        fprintf(stdout, "ERROR %s\n", "Failed to read pipe");
        return -1;
    }
    if (n_boxes == 0) {
        send_box_manager(client_named_pipe_path, '1', NULL);
    }
    int count = 0;
    for (int i = 0; i < max_sessions; i++) {
        if (strcmp("", boxes[i].name)) {
            count++;
            if (count == n_boxes) {
                send_box_manager(client_named_pipe_path, '1', &boxes[i]);
                break;
            }
            send_box_manager(client_named_pipe_path, '0', &boxes[i]);
        }
    }
    fprintf(stdout, "OK\n");
    return 0;
}


/* void *main_thread_func(void *arg) {
    pcq_create()
    return NULL;
}

void *thread2_func(void *arg) {
    
    return NULL;
}

void *thread3_func(void *arg) {
    
    return NULL;
}                                                                                   CONTINUO SEM SABER SE É ISSO

void *thread4_func(void *arg) {
    
    return NULL;
}

void *thread5_func(void *arg) {
    
    return NULL;
}
 */



void function(int parser_fn(worker_t*), char op_code) {
    int session_id = -1;
    for (int i = 0; i < max_sessions; i++) {
        if (!workers[i].to_execute) {
            session_id = i;
            break;
        }
    }
    if (session_id < 0) {
        fprintf(stdout, "ERROR %s\n", "No sessions left");
    }

    worker_t *worker = &workers[session_id];

    pcq_enqueue(&queue, worker);
    
    pthread_mutex_lock(&worker->lock);
    
    worker->packet.opcode = op_code;
    int result = 0;
    if (parser_fn != NULL) {
        result = parser_fn(worker);
    }
    pthread_mutex_lock(&register_pipe_lock);
    if (result == 0) {
        worker->to_execute = true;
        if (pthread_cond_signal(&worker->cond) != 0) {
            fprintf(stdout, "ERROR %s\n", "Couldn't signal worker");
        }
    } else {
        fprintf(stdout, "ERROR %s\n", "Failed to execute worker");
    }

    pthread_mutex_unlock(&worker->lock);
}

int main(int argc, char **argv) {

    /* pthread_t thread1, thread2, thread3, thread4, thread5;

    pthread_create(&thread1, NULL, thread1_func, NULL);
    pthread_create(&thread2, NULL, thread2_func, NULL);
    pthread_create(&thread3, NULL, thread3_func, NULL);
    pthread_create(&thread4, NULL, thread4_func, NULL);
    pthread_create(&thread5, NULL, thread5_func, NULL);
                                                                                  IDK SE É SUPOSTO FAZER ISSO
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);
    pthread_join(thread4, NULL);
    pthread_join(thread5, NULL); */

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
    char op_code;
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
                //printf("PUBLISHER: ");
                //start_publisher();
                function(NULL, op_code);
                break;
            case OP_CODE_SUBSCRIBER:
                //printf("SUBSCRIBER: ");
                //start_subscriber();
                function(NULL, op_code);
                break;
            case OP_CODE_BOX_CREATE:
                //printf("BOX_CREATE: ");
                //create_box();
                function(NULL, op_code);
                break;
            case OP_CODE_BOX_REMOVE:
                //printf("BOX_REMOVE: ");
                //remove_box();
                function(NULL, op_code);
                break;
            case OP_CODE_BOX_LIST:
                //printf("BOX_LIST: ");
                //list_boxes();
                function(NULL, op_code);
                break;
            default:
                printf("SWITCH CASE DOES NOT WORK\n"); //print para debug
                break;
            }

            if (pthread_mutex_lock(&register_pipe_lock) != 0) {
                destroy_server(EXIT_FAILURE);
            }

            do {
                bytes_read = read(register_pipe, &op_code, sizeof(char));
            } while (bytes_read < 0 && errno == EINTR);

            if (pthread_mutex_unlock(&register_pipe_lock) != 0) {
                destroy_server(EXIT_FAILURE);
            }
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
