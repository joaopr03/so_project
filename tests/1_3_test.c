#include "fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

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

void wait_file_create(char const *path) {
    int i;
    while ((i = tfs_open(path, 0)) == -1);
    assert(tfs_close(i) != -1);  
}

void *client1() {
    printf("Client1 start\n");

    // Client1 creates 3 files and writes on them
    write_contents("/file1", "Good night.");
    write_contents("/file2", "My cat is adorable!");
    write_contents("/file3", "Why would Client1 create a file without text...");

    printf("Client1 end\n");
    return NULL;
}

void *client2() {
    printf("Client2 start\n");

    // Client2 starts by deleting file1
    wait_file_create("/file1");    
    assert(tfs_unlink("/file1") != -1);
    
    // Client2 creates file1 again, write on it and then verifies
    // if the content he has written is what he actually wrote
    assert_contents_ok("/file1", "");
    write_contents("/file1", "Client1 is not a good friend.");
    assert_contents_ok("/file1", "Client1 is not a good friend.");
    
    printf("Client2 end\n");
    return NULL;
}

void *client3() {
    printf("Client3 start\n");

    char *hard_link1 = "/hard1";
    char *hard_link2 = "/hard2";
    char *hard_link3 = "/hard3";
    
    char *soft_link1 = "/soft1"; 
    char *soft_link2 = "/soft2";

    // Create 3 hard links with the same content written
    write_contents(hard_link1, "A new beginning!");
    assert(tfs_link(hard_link1, hard_link2) != -1);
    assert(tfs_link(hard_link2, hard_link3) != -1);
    assert_contents_ok(hard_link1, "A new beginning!");
    assert_contents_ok(hard_link2, "A new beginning!");
    assert_contents_ok(hard_link3, "A new beginning!");

    // Create 2 soft links (one from another hard link)
    assert(tfs_sym_link(hard_link2, soft_link1) != -1);
    assert(tfs_sym_link(soft_link1, soft_link2) != -1);
    assert_contents_ok(soft_link1, "A new beginning!");
    assert_contents_ok(soft_link2, "A new beginning!");

    // Expected return -1, soft link open
    assert(tfs_unlink(soft_link1) != -1);
    assert(tfs_open(soft_link2, 0) == -1);

    // Link soft link from other hard link
    assert(tfs_unlink(soft_link2) != -1);
    assert(tfs_sym_link(hard_link3, soft_link2) != -1);
    assert_contents_ok(soft_link2, "A new beginning!");

    // Unlink 2 hard links (inode count == 1)
    assert(tfs_unlink(hard_link1) != -1);
    assert(tfs_unlink(hard_link2) != -1);
    assert_contents_ok(hard_link3, "A new beginning!");
    assert_contents_ok(soft_link2, "A new beginning!");

    // Unlink all hard links (expected delete inode, inode count == 0)
    assert(tfs_unlink(hard_link3) != -1);
    assert_contents_ok(hard_link3, "");
    
    printf("Client3 end\n");
    return NULL;
}

void *client4() {
    printf("Client4 start\n");

    // Client4 creates a soft link and verifies its content
    wait_file_create("/file2");
    assert(tfs_sym_link("/file2", "/softL") != -1);
    assert_contents_ok("/softL", "My cat is adorable!");
    
    // Client4 creates a hard link, verifies its content and then changes it
    assert(tfs_link("/file2", "/hardL") != -1);
    assert_contents_ok("/hardL", "My cat is adorable!");
    write_contents("/hardL", "Don't be mean, Client2.");

    // Client4 verifies the content of the soft link
    assert_contents_ok("/softL", "Don't be mean, Client2.");
    
    // Client4 verifies the content of file2 and its hard link
    assert_contents_ok("/file2", "Don't be mean, Client2.");
    assert_contents_ok("/hardL", "Don't be mean, Client2.");

    printf("Client4 end\n");
    return NULL;
}

void *client5() {
    printf("Client5 start\n");

    char *str_ext_file =
        "Texto de ficheiro de teste. Teste TecnicoFS1. Texto de ficheiro de teste. Teste TecnicoFS11."
        "Texto de ficheiro de teste. Teste TecnicoFS2. Texto de ficheiro de teste. Teste TecnicoFS22."
        "Texto de ficheiro de teste. Teste TecnicoFS3. Texto de ficheiro de teste. Teste TecnicoFS33."
        "Texto de ficheiro de teste. Teste TecnicoFS4. Texto de ficheiro de teste. Teste TecnicoFS44."
        "Texto de ficheiro de teste. Teste TecnicoFS5. Texto de ficheiro de teste. Teste TecnicoFS55."
        "Texto de ficheiro de teste. Teste TecnicoFortnite6. Texto de ficheiro de teste. Teste TecnicoFS66."
        "Texto de ficheiro de teste. Teste TecnicoFS7. Texto de ficheiro de teste. Teste TecnicoFS77."
        "Texto de ficheiro de teste. Teste TecnicoFS8. Texto de ficheiro de teste. Teste TecnicoFS."
        "Texto de ficheiro de teste. Teste TecnicoFS9. Texto de ficheiro de teste. Teste TecnicoFS."
        "Texto de ficheiro de teste. Teste TecnicoFS0. Texto de ficheiro de teste. Teste TecnicoFS."
        "Texto de ficheiro de teste. Teste TecnicoFS1. Texto de ficheiro de teste. Teste TecnicoFS.";
    char *path_src = "tests/file_to_copy_from_test.txt";
    ssize_t r;
    int f;
    char buffer[1020];
    
    FILE *file = fopen(path_src, "w");
    fwrite(str_ext_file, sizeof(*str_ext_file), strlen(str_ext_file), file);
    fclose(file);

    // Create a file (file does not exist)
    f = tfs_copy_from_external_fs(path_src, "/file19");
    assert(f != -1);

    f = tfs_open("/file19", TFS_O_CREAT);
    assert(f != -1);

    // Check tfs file content
    r = tfs_read(f, buffer, sizeof(buffer) - 1);
    assert(r == strlen(str_ext_file));
    assert(!memcmp(buffer, str_ext_file, strlen(str_ext_file)));

    
    printf("Client5 end\n");
    return NULL;
}

void *client6() {
    printf("Client6 start\n");

    // Client6 create soft link to file3 and verifies its content
    wait_file_create("/file3");
    assert(tfs_sym_link("/file3", "/softLink") != -1);
    assert_contents_ok("/softLink", "Why would Client1 create a file without text...");

    // Client6 deletes file3 created by Client1
    assert(tfs_unlink("/file3") != -1);

    // Client6 tries to open and delete (by this order) files that don't exist
    assert(tfs_open("/softLink", 0) == -1);
    assert(tfs_unlink("/file3") == -1);    
    
    printf("Client6 end\n");
    return NULL;
}



int main() {
    pthread_t tid[6];

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

    if (pthread_create(&tid[5], NULL, client6, NULL) != 0) {
        fprintf(stderr, "failed to create client2 thread: %s\n", strerror(errno));
        return -1;
    }

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);
    pthread_join(tid[2], NULL);
    pthread_join(tid[3], NULL);
    pthread_join(tid[4], NULL);
    pthread_join(tid[5], NULL);

    assert(tfs_destroy() != -1);

    printf("Successful test.\n");
    return 0;
}
