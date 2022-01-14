#include "operations.h"
#include <assert.h>

int filehandle1;
int filehandle2;

void* thr_func(void* ptr) {
    filehandle2 = tfs_open("/f2", 0);
    assert(tfs_close(filehandle2) != -1);
    return NULL;
}

int main() {
    //Multiple threads try to tfs_open the same file. Find whether the file handlers are different.

    char input[12] = "12345678901";
    assert(tfs_init() != -1);
    int fd = tfs_open("/f2", TFS_O_CREAT);
    assert(fd != -1);
    assert(tfs_write(fd, input, 12) == 12);
    assert(tfs_close(fd) != -1);

    pthread_t tid;
    pthread_create(&tid, NULL, thr_func, NULL);

    filehandle1 = tfs_open("/f2", 0);
    printf("First thread's file handle: %d\n", filehandle1);

    pthread_join(tid, NULL);
    printf("Second thread's file handle: %d\n", filehandle2);

    assert(filehandle1 != filehandle2);

    assert(tfs_close(filehandle1) != -1);
    assert(tfs_destroy() != -1);
    printf("Successful test!\n");

    return 0;
}