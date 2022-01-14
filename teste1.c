#include "operations.h"
#include <assert.h>
#include <string.h>

int filehandle1;
int filehandle2;
int filehandle3;
char input1[8] = "1234567";
char input2[5] = "abcd";

void* thr_func(void* ptr) {
    filehandle2 = tfs_open("/f3", 0);
    assert(filehandle2 != -1);
    assert(tfs_write(filehandle2, input2, 4) == 4);
    assert(tfs_close(filehandle2) != -1);
    return NULL;
}

int main() {
    //Different threads open a file, resulting in different file handles.
    //tfs_write is thus performed with different file handles

    assert(tfs_init() != -1);
    filehandle1 = tfs_open("/f3", TFS_O_CREAT);
    assert(filehandle1 != -1);
    assert(tfs_close(filehandle1) != -1);

    pthread_t tid;
    pthread_create(&tid, NULL, thr_func, NULL);

    filehandle1 = tfs_open("/f3", 0);
    assert(filehandle1 != -1);
    assert(tfs_write(filehandle1, input1, 7) == 7);
    assert(tfs_close(filehandle1) != -1);

    pthread_join(tid, NULL);

    char buffer[20];
    filehandle3 = tfs_open("/f3", 0);
    assert(tfs_read(filehandle3, buffer, 20) == 7);
    assert(tfs_close(filehandle3) != -1);

    assert(tfs_destroy() != -1);
    printf("Successful test!\n");

    return 0;
}