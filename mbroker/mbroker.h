#ifndef __MBROKER_H__
#define __MBROKER_H__

#include <pthread.h>

#include "logging.h"
#include "operations.h"
#include "producer-consumer.h"
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
    OP_CODE_BOX_LIST_R = '8'
};

typedef struct {
    char opcode;
    int session_id;
    bool to_execute;
    pthread_t tid;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} worker_t;

typedef struct {
    char name[BOX_NAME_SIZE];
    uint64_t size;
    uint64_t n_publishers;
    uint64_t n_subscribers;
    pthread_mutex_t lock;
} box_t;

void destroy_server(int status);

static void sig_handler(int sig);

void *session_worker(void *args);

int init_server();

int process_entry(char *client, char *box);

int register_entry(char *client_named_pipe_path, char *box_name);

int start_publisher();

int start_subscriber();

int send_message_manager(const char *pipe_path, char operation, int32_t code, char *message);

int send_box_manager(const char *pipe_path, char last, box_t *box);

int create_box();

int remove_box();

int list_boxes();

void function(int parser_fn(worker_t*), char op_code);

#endif // __MBROKER_H__
