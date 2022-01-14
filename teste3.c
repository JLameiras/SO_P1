#include "operations.h"
#include <assert.h>
#include <string.h>

int filehandle;
char input1[5] = "1234";
char input2[5] = "abcd";

void* thr_func(void* ptr) {
    assert(tfs_write(filehandle, input2, 4) == 4);
    return NULL;
}

int main() {
    //Two threads call tfs_write with the same file handle.
    assert(tfs_init() != -1);
    filehandle = tfs_open("/f3", TFS_O_CREAT);
    assert(filehandle != -1);

    pthread_t tid;
    pthread_create(&tid, NULL, thr_func, NULL);

    assert(tfs_write(filehandle, input1, 4) == 4);

    pthread_join(tid, NULL);
    assert(tfs_close(filehandle) != -1);

    char buffer[20];
    filehandle = tfs_open("/f3", 0);
    assert(tfs_read(filehandle, buffer, 20) == 8);
    assert(tfs_close(filehandle) != -1);

    assert(tfs_destroy() != -1);
    printf("Successful test!\n");

    return 0;
}