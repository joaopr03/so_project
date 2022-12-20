#include "fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void assert_contents_ok(char const *path, char const *file_contents) {
    int f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    char buffer[strlen(file_contents)];
    assert(tfs_read(f, buffer, sizeof(buffer)) == sizeof(buffer));
    assert(memcmp(buffer, file_contents, sizeof(buffer)) == 0);

    assert(tfs_close(f) != -1);
}

void write_contents(char const *path, char const *file_contents) {
    int f = tfs_open(path, TFS_O_CREAT);
    assert(f != -1);

    assert(tfs_write(f, file_contents, strlen(file_contents)+1) == strlen(file_contents)+1);

    assert(tfs_close(f) != -1);
}

int main() {
    char const str1[] = "AAAAAAAbAAAAAAAAAa";
    char const str2[] = "Hello world";
    char const str3[] = "A new beginning!";
    char const empty_string[] = "";

    char *hard_link1 = "/hl1";
    char *hard_link2 = "/hl2";
    char *hard_link3 = "/hl3";
    
    char *soft_link1 = "/sl1";
    char *soft_link2 = "/sl2"; 
    char *soft_link3 = "/sl3";
 
    // Initialize TecnicoFS
    assert(tfs_init(NULL) != -1); 

    // Create a hard link, write content on it and then delete it
    write_contents(hard_link1, str1);
    assert_contents_ok(hard_link1, str1);
    assert(tfs_unlink(hard_link1) != -1);
    assert_contents_ok(hard_link1, empty_string);

    // Create a hard link from a soft link 
    write_contents(hard_link1, str2);
    assert(tfs_sym_link(hard_link1, soft_link1) != -1);
    assert_contents_ok(soft_link1, str2);
    assert(tfs_link(soft_link1, hard_link2) == -1);
    assert_contents_ok(hard_link2, empty_string);
    
    // Check if inode is the same
    write_contents(hard_link2, str2);
    assert(tfs_link(hard_link2, hard_link3) != -1);
    assert(tfs_unlink(hard_link2) != -1);
    assert_contents_ok(hard_link3, str2);

    // Unlink linked hard links 
    assert(tfs_unlink(hard_link1) != -1);
    assert(tfs_unlink(hard_link3) != -1);
    
    // Create 3 hard links with the same content written
    write_contents(hard_link1, str3);
    assert(tfs_link(hard_link1, hard_link2) != -1);
    assert(tfs_link(hard_link2, hard_link3) != -1);
    assert_contents_ok(hard_link1, str3);
    assert_contents_ok(hard_link2, str3);
    assert_contents_ok(hard_link3, str3);

    // Create 2 soft links (one from another hard link)
    assert(tfs_sym_link(hard_link2, soft_link2) != -1);
    assert(tfs_sym_link(soft_link2, soft_link3) != -1);
    assert_contents_ok(soft_link2, str3);
    assert_contents_ok(soft_link3, str3);

    // Expected return -1, soft link open
    assert(tfs_unlink(hard_link2) != -1);
    assert(tfs_open(soft_link2, 0) == -1);
    assert(tfs_open(soft_link3, 0) == -1);

    // Create hard link again
    assert(tfs_link(hard_link3, hard_link2) != -1);
    assert_contents_ok(soft_link2, str3);
    assert_contents_ok(soft_link3, str3);

    // Expected return -1, soft link open
    assert(tfs_unlink(soft_link2) != -1);
    assert(tfs_open(soft_link3, 0) == -1);

    // Link soft link from other hard link
    assert(tfs_unlink(soft_link3) != -1);
    assert(tfs_sym_link(hard_link3, soft_link3) != -1);
    assert_contents_ok(soft_link3, str3);

    // Unlink 2 hard links (inode count == 1)
    assert(tfs_unlink(hard_link1) != -1);
    assert(tfs_unlink(hard_link2) != -1);
    assert_contents_ok(hard_link3, str3);
    assert_contents_ok(soft_link3, str3);

    // Unlink all hard links (expected delete inode, inode count == 0)
    assert(tfs_unlink(hard_link3) != -1);
    assert_contents_ok(hard_link3, empty_string);

    assert(tfs_destroy() != -1);
    
    printf("Successful test.\n");

    return 0;
}
