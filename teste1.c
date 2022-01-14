#include "operations.h"
#include <assert.h>
#include <string.h>

int main() {

    char *path = "/f1";
    char input1[12] = "abcdefghjik";
    char input2[11] = "1345362739"

    assert(tfs_init() != -1);

    int fd = tfs_open(path, TFS_O_CREAT);
    assert(fd != -1);
    tfs_write(fd, input, 12);

    tfs_open(path, TFS_O_APPEND);
    tfs_open(path, TFS_O_APPEND);

    printf("Successful test.\n");

    return 0;
}