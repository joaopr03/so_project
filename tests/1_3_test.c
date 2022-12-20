#include "fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

//#include <state.h>

void write_contents(char const *path, char const *file_contents) {
    int f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    assert(tfs_write(f, file_contents, strlen(file_contents)+1) == strlen(file_contents)+1);

    assert(tfs_close(f) != -1);
}

void assert_contents_ok(char const *path, char const *file_contents) {
    int f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    char buffer[strlen(file_contents)];
    assert(tfs_read(f, buffer, sizeof(buffer)) == sizeof(buffer));
    assert(memcmp(buffer, file_contents, sizeof(buffer)) == 0);

    assert(tfs_close(f) != -1);
}

void *client1() {
    printf("Client1 start\n");

    write_contents("/file1", "Good night.");

    write_contents("/file2", "My cat is adorable!");

    write_contents("/file3", "");

    printf("Client1 end\n");
    return NULL;
}

void *client2() {
    printf("Client2 start\n");

    int a;
    while((a = tfs_open("/file1", 0)) == -1);
    assert(a != -1);
    
    char buffer[strlen("Good night.")];
    assert(tfs_read(a, buffer, sizeof(buffer)) == sizeof(buffer));
    assert(memcmp(buffer, "Good night.", sizeof(buffer)) == 0);
    
    assert(tfs_unlink("/file1") != -1);
    
    assert_contents_ok("/file1", "");
    write_contents("/file1", "Client1 is not a good friend.");
    
    printf("Client2 end\n");
    return NULL;
}

void *client3() {
    printf("Client3 start\n");
    
    assert(tfs_unlink("/file2") != -1);
    printf("Client3 end\n");
    return NULL;
}

void *client4() {
    printf("Client4 start\n");

    int andle_withou_h;
    while((andle_withou_h = tfs_open("/hard3", 0)) == -1);
    /* assert(tfs_close(andle_withou_h) != -1);
    write_contents("/hard3", "IDK"); */
    
    printf("Client4 end\n");
    return NULL;
}

void *client5() {
    printf("Client5 start\n");
    char const str1[] = "A new beginning!";
    char const empty_string[] = "";

    char *hard_link1 = "/hard1";
    char *hard_link2 = "/hard2";
    char *hard_link3 = "/hard3";
    
    char *soft_link1 = "/soft1"; 
    char *soft_link2 = "/soft2";
    // Create 3 hard links with the same content written
    write_contents(hard_link1, str1);
    assert(tfs_link(hard_link1, hard_link2) != -1);
    assert(tfs_link(hard_link2, hard_link3) != -1);
    assert_contents_ok(hard_link1, str1);
    assert_contents_ok(hard_link2, str1);
    assert_contents_ok(hard_link3, str1);

    // Create 2 soft links (one from another hard link)
    assert(tfs_sym_link(hard_link2, soft_link1) != -1);
    assert(tfs_sym_link(soft_link1, soft_link2) != -1);
    assert_contents_ok(soft_link1, str1);
    assert_contents_ok(soft_link2, str1);

    // Expected return -1, soft link open
    assert(tfs_unlink(soft_link1) != -1);
    assert(tfs_open(soft_link2, 0) == -1);

    // Link soft link from other hard link
    assert(tfs_unlink(soft_link2) != -1);
    assert(tfs_sym_link(hard_link3, soft_link2) != -1);
    assert_contents_ok(soft_link2, str1);

    // Unlink 2 hard links (inode count == 1)
    assert(tfs_unlink(hard_link1) != -1);
    assert(tfs_unlink(hard_link2) != -1);
    assert_contents_ok(hard_link3, str1);
    assert_contents_ok(soft_link2, str1);

    // Unlink all hard links (expected delete inode, inode count == 0)
    assert(tfs_unlink(hard_link3) != -1);
    assert_contents_ok(hard_link3, empty_string);
    
    printf("Client5 end\n");
    
    return NULL;
}

int main() {
    pthread_t tid[5];

    assert(tfs_init(NULL) != -1);
    
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

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);
    pthread_join(tid[2], NULL);
    pthread_join(tid[3], NULL);
    pthread_join(tid[4], NULL);

    assert(tfs_destroy() != -1);

    printf("Successful test.\n");

    return 0;
}
