#include "fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

//#include <state.h>

/* void *client1() {
    printf("Client1 start\n");
    tfs_init(NULL);
    tfs_destroy();
    tfs_open("ola", 0);
    tfs_destroy();
    tfs_init(NULL);
    tfs_destroy();
    tfs_open("ola", 0);
    tfs_destroy();
    tfs_init(NULL);
    tfs_destroy();
    tfs_open("ola", 0);
    tfs_destroy();
    tfs_init(NULL);
    tfs_destroy();
    tfs_open("ola", 0);
    tfs_destroy();
    printf("Client1 end\n");
    return NULL;
}

void *client2() {
    printf("Client2 start\n");
    tfs_init(NULL);
    tfs_destroy();
    tfs_open("ola", 0);
    tfs_destroy();
    printf("Client2 end\n");
    return NULL;
}

void *client3() {
    char *path = "/merda";
    printf("Client3 start\n");
    tfs_init(NULL);
    tfs_destroy();
    tfs_init(NULL);
    while (tfs_open(path, TFS_O_CREAT) == -1);
        //inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
        //tfs_lookup(path, root_dir_inode);
    //}
    tfs_destroy();
    tfs_open("ola", 0);
    tfs_destroy();
    tfs_open("ola", 0);
    tfs_destroy();
    printf("Client3 end\n");
    return NULL;
}

void *client4() {
    printf("Client4 start\n");
    tfs_init(NULL);
    tfs_destroy();    
    printf("Client4 end\n");
    return NULL;
}

void *client5() {
    printf("Client5 start\n");
    tfs_init(NULL);
    tfs_destroy();
    printf("Client5 end\n");
    return NULL;
}
 */
/* void *client6() {
    printf("Client6 start\n");
    tfs_init(NULL);
    tfs_destroy();    
    printf("Client6 end\n");
    return NULL;
}

void *client7() {
    char *path = "/merda";
    printf("Client7 start\n");
    tfs_init(NULL);
    while (tfs_open(path, TFS_O_CREAT) == -1);
        //inode_t *root_dir_inode = inode_get(ROOT_DIR_INUM);
        //tfs_lookup(path, root_dir_inode);
    //}
    tfs_destroy();   
    printf("Client7 end\n"); 
    return NULL;
}

void *client8() {
    printf("Client8 start\n");
    tfs_init(NULL);
    tfs_destroy();  
    printf("Client8 end\n");  
    return NULL;
}

void *client9() {
    printf("Client9 start\n");
    tfs_init(NULL);
    tfs_destroy();  
    printf("Client9 end\n");  
    return NULL;
}

void *client10() {
    printf("Client10 start\n");
    tfs_init(NULL);
    tfs_destroy();   
    printf("Client10 end\n"); 
    return NULL;
} */

int main() {
/*     pthread_t tid[10];

    if (pthread_create(&tid[0], NULL, client1, NULL) != 0) {
        fprintf(stderr, "failed to create client1 thread: %s\n", strerror(errno));
        return -1;
    }
    if (pthread_create(&tid[1], NULL, client2, NULL) != 0) {
        fprintf(stderr, "failed to create client2 thread: %s\n", strerror(errno));
        return -1;
    }
    if (pthread_create(&tid[2], NULL, client3, NULL) != 0) {
        fprintf(stderr, "failed to create client2 thread: %s\n", strerror(errno));
        return -1;
    }
    if (pthread_create(&tid[3], NULL, client4, NULL) != 0) {
        fprintf(stderr, "failed to create client2 thread: %s\n", strerror(errno));
        return -1;
    }
    if (pthread_create(&tid[4], NULL, client5, NULL) != 0) {
        fprintf(stderr, "failed to create client2 thread: %s\n", strerror(errno));
        return -1;
    }
    if (pthread_create(&tid[5], NULL, client6, NULL) != 0) {
        fprintf(stderr, "failed to create client2 thread: %s\n", strerror(errno));
        return -1;
    }
    if (pthread_create(&tid[6], NULL, client7, NULL) != 0) {
        fprintf(stderr, "failed to create client2 thread: %s\n", strerror(errno));
        return -1;
    }
    if (pthread_create(&tid[7], NULL, client8, NULL) != 0) {
        fprintf(stderr, "failed to create client2 thread: %s\n", strerror(errno));
        return -1;
    }
    if (pthread_create(&tid[8], NULL, client9, NULL) != 0) {
        fprintf(stderr, "failed to create client2 thread: %s\n", strerror(errno));
        return -1;
    }
    if (pthread_create(&tid[9], NULL, client10, NULL) != 0) {
        fprintf(stderr, "failed to create client2 thread: %s\n", strerror(errno));
        return -1;
    } 

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);
    pthread_join(tid[2], NULL);
    pthread_join(tid[3], NULL);
    pthread_join(tid[4], NULL);
    pthread_join(tid[5], NULL);
    pthread_join(tid[6], NULL);
    pthread_join(tid[7], NULL);
    pthread_join(tid[8], NULL);
    pthread_join(tid[9], NULL);
 */
    printf("Successful test.\n");

    return 0;
}
