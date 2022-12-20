#include "fs/operations.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main() {

    char *str_ext_file1 =
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

    char *str_ext_file2 = "Hello world!";
    char *path_copied_file1 = "/teste1a";
    char *path_copied_file2 = "/teste1b";
    char *path_src = "tests/file_to_copy_from_test.txt";
    char buffer[1020];
    ssize_t r;
    int f;

    FILE *file = fopen(path_src, "w");
    fwrite(str_ext_file1, sizeof(*str_ext_file1), strlen(str_ext_file1), file);
    fclose(file);

    assert(tfs_init(NULL) != -1);

    // Create a file (file does not exist)
    f = tfs_copy_from_external_fs(path_src, path_copied_file1);
    assert(f != -1);

    f = tfs_open(path_copied_file1, TFS_O_CREAT);
    assert(f != -1);

    // Check tfs file content
    r = tfs_read(f, buffer, sizeof(buffer) - 1);
    assert(r == strlen(str_ext_file1));
    assert(!memcmp(buffer, str_ext_file1, strlen(str_ext_file1)));

    // Create a new file with some content
    f = tfs_open(path_copied_file2, TFS_O_CREAT);
    assert(f != -1);

    r = tfs_write(f, str_ext_file2, strlen(str_ext_file2)+1);
    assert(r == strlen(str_ext_file2)+1);
    
    f = tfs_close(f);
    assert(f != -1);

    // Replace file content (file already exists)
    f = tfs_copy_from_external_fs(path_src, path_copied_file2);
    assert(f != -1);

    f = tfs_open(path_copied_file2, TFS_O_CREAT);
    assert(f != -1);

    // Check tfs file content
    r = tfs_read(f, buffer, sizeof(buffer) - 1);
    assert(r == strlen(str_ext_file1));
    assert(!memcmp(buffer, str_ext_file1, strlen(str_ext_file1)));


    file = fopen(path_src, "w");
    fwrite(str_ext_file2, sizeof(*str_ext_file2), strlen(str_ext_file2), file);
    fclose(file);

    // Replace file content (file already exists) 
    f = tfs_copy_from_external_fs(path_src, path_copied_file1);
    assert(f != -1);

    f = tfs_open(path_copied_file1, TFS_O_CREAT);
    assert(f != -1);

    // Check tfs file content
    r = tfs_read(f, buffer, sizeof(buffer) - 1);
    assert(r == strlen(str_ext_file2));
    assert(!memcmp(buffer, str_ext_file2, strlen(str_ext_file2)));

    printf("Successful test.\n");

    return 0;
}