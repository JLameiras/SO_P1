#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include<pthread.h>
#include<unistd.h>

int tfs_init() {
    state_init();

    /* create root inode */
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    state_destroy();
    return 0;
}

static bool valid_pathname(char const *name) {
    /*Each name has to be checked
     * Suggestion: splice string by character '/' and check if name is valid by traversing the file system according to
     * the given path */
    for(int i = 1; i < strlen(name); i++) if(name[i] == '/') return false;
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}


int tfs_lookup(char const *name) {
    if (!valid_pathname(name)) {
        return -1;
    }

    // skip the initial '/' character
    name++;

    return find_in_dir(ROOT_DIR_INUM, name);
}

int tfs_open(char const *name, int flags) {
    int inum;
    size_t offset;

    /* Checks if the path name is valid */
    if (!valid_pathname(name)) {
        return -1;
    }

    inum = tfs_lookup(name);
    if (inum >= 0) {
        /* The file already exists */
        inode_t *inode = inode_get(inum);
        if (inode == NULL) {
            return -1;
        }

        /* Trucate (if requested) */
        if (flags & TFS_O_TRUNC) {
            pthread_rwlock_wrlock(&inode->rwlock);
            if (inode->i_size > 0) {
                for(int i = 0; i < 10; i++)
                    if(inode->i_data_block[i] != -1 && data_block_free(inode->i_data_block[i]) == -1){
                        pthread_rwlock_unlock(&inode->rwlock);
                        return -1;
                    }
                int *blocks;
                if((blocks = (int*)data_block_get(inode->supp_block)) != NULL) {
                    for (int i = 0; i < BLOCK_SIZE; i += sizeof(int))
                        if (*(blocks + i) != -1 && data_block_free(*(blocks + i)) == -1){
                            pthread_rwlock_unlock(&inode->rwlock);
                            return -1;
                        }
                    if(data_block_free((inode->supp_block)) == -1){
                        pthread_rwlock_unlock(&inode->rwlock);
                        return -1;
                    }
                }
                inode->i_size = 0;
            }
            pthread_rwlock_unlock(&inode->rwlock);
        }
        /* Determine initial offset */
        if (flags & TFS_O_APPEND) {
            pthread_rwlock_rdlock(&inode->rwlock);
            offset = inode->i_size;
            pthread_rwlock_unlock(&inode->rwlock);
        } else {
            offset = 0;
        }
    } else if (flags & TFS_O_CREAT) {
        /* The file doesn't exist; the flags specify that it should be created*/
        /* Create inode */
        inum = inode_create(T_FILE);
        if (inum == -1) {
            return -1;
        }
        /* Add entry in the root directory */
        if (add_dir_entry(ROOT_DIR_INUM, inum, name + 1) == -1) {
            inode_delete(inum);
            return -1;
        }
        offset = 0;
    } else {
        return -1;
    }

    /* Finally, add entry to the open file table and
     * return the corresponding handle */
    return add_to_open_file_table(inum, offset);

    /* Note: for simplification, if file was created with TFS_O_CREAT and there
     * is an error adding an entry to the open file table, the file is not
     * opened but it remains created */
}


int tfs_close(int fhandle) { return remove_from_open_file_table(fhandle); }

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {

    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }
    pthread_rwlock_wrlock(&file->rwlock);


    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        pthread_rwlock_unlock(&file->rwlock);
        return -1;
    }
    pthread_rwlock_rdlock(&inode->rwlock);

    int written = 0;
    int block_dif = 0;
    int block_write = 0;

    if (to_write > 0) {
        int current_block = file->of_offset / BLOCK_SIZE;

        if(current_block < 10) {
            for(int i = current_block; i < 10 && to_write != written; i++) {
                // Check if block exists, if not create one. Get block content pointer
                if(inode->i_data_block[i] == -1)
                    inode->i_data_block[i] = data_block_alloc();
                void *block = data_block_get(inode->i_data_block[i]);
                // Check if block pointer is valid
                if(block == NULL) {
                    pthread_rwlock_unlock(&file->rwlock);
                    pthread_rwlock_unlock(&inode->rwlock);
                    return -1;
                }
                // Get amount of chars to write onto block
                block_dif = BLOCK_SIZE - file->of_offset % BLOCK_SIZE;;
                block_write = block_dif > to_write - written ? to_write - written : block_dif;
                // Write onto block and update state
                memcpy(block + file->of_offset % BLOCK_SIZE, buffer, block_write);
                written += block_write;
                file->of_offset += block_write;
            }
            if (file->of_offset > inode->i_size) {
                inode->i_size = file->of_offset;
            }
            current_block = 10;
        }

        if(to_write != written){
            int *block_content = (int *) data_block_get(inode->supp_block);
            if(block_content == NULL) { // Check content pointer
                pthread_rwlock_unlock(&file->rwlock);
                pthread_rwlock_unlock(&inode->rwlock);
                return -1;
            }
            int begin = current_block - 10;
            for(int i = begin * sizeof(int); i < BLOCK_SIZE && to_write != written; i += sizeof(int)) {
                // Check if block exists, if not create one. Get pointer to block content
                if(*(block_content + i) == -1)
                    *(block_content + i) = data_block_alloc();
                void *block = data_block_get(*(block_content + i));
                // Check if block pointer is valid
                if (block == NULL) {
                    pthread_rwlock_unlock(&file->rwlock);
                    pthread_rwlock_unlock(&inode->rwlock);
                    return -1;
                }
                // Get amount of chars to write onto block
                block_dif = BLOCK_SIZE - file->of_offset % BLOCK_SIZE;
                block_write = block_dif > to_write - written ? to_write - written : block_dif;
                /* Perform the actual write */
                memcpy(block + file->of_offset % BLOCK_SIZE, buffer, block_write);
                /* Update state*/
                file->of_offset += block_write;
                written += block_write;
            }
        }
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }
    }
    pthread_rwlock_unlock(&file->rwlock);
    pthread_rwlock_unlock(&inode->rwlock);
    return (ssize_t)written;
}


ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }
    pthread_rwlock_wrlock(&file->rwlock);

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        pthread_rwlock_unlock(&file->rwlock);
        return -1;
    }
    pthread_rwlock_rdlock(&inode->rwlock);

    /* Determine how many bytes to read */
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }

    int read = 0;
    int block_dif, block_read;

    if (to_read > 0) {
        /* Determining the block in which the data is stored is non trivial */
        for(int i = (int)file->of_offset / BLOCK_SIZE; i < 10 && to_read != read; i++) {
            void *block = data_block_get(inode->i_data_block[i]);
            if (block == NULL) {
                pthread_rwlock_unlock(&file->rwlock);
                pthread_rwlock_unlock(&inode->rwlock);
                return -1;
            }

            block_dif = BLOCK_SIZE - file->of_offset % BLOCK_SIZE;
            block_read = block_dif > ((int)to_read - read) ? ((int)to_read - read) : block_dif;

            /* Perform the actual read */
            memcpy(buffer, block + (file->of_offset % BLOCK_SIZE), block_read);

            /* The offset associated with the file handle is
            * incremented accordingly */
            file->of_offset += block_read;
            read += block_read;
        }
        if(to_read != read){
            int *block_content = (int *) data_block_get(inode->supp_block);
            int begin = (int)file->of_offset / BLOCK_SIZE - 10;
            for(int i = begin * sizeof(int); i < BLOCK_SIZE && to_read != read; i += sizeof(int)) {
                void *block = data_block_get(*(block_content + i));
                if (block == NULL) {
                    pthread_rwlock_unlock(&file->rwlock);
                    pthread_rwlock_unlock(&inode->rwlock);
                    return -1;
                }

                block_dif = BLOCK_SIZE - file->of_offset % BLOCK_SIZE;
                block_read = block_dif > ((int)to_read - read) ? ((int)to_read - read) : block_dif;

                /* Perform the actual read */
                memcpy(buffer, block + (file->of_offset % BLOCK_SIZE), block_read);

                /* The offset associated with the file handle is
                * incremented accordingly */
                file->of_offset += block_read;
                read += block_read;
            }
        }
    }
    pthread_rwlock_unlock(&file->rwlock);
    pthread_rwlock_unlock(&inode->rwlock);
    return (ssize_t)read;
}

int tfs_copy_to_external_fs(char const *source_path, char const *dest_path) {
    // Get file in TFS
    int inumb;
    if((inumb = tfs_lookup(source_path)) == -1 || inumb == 0)
        return -1;
    inode_t *inode = inode_get(inumb);
    pthread_rwlock_rdlock(&inode->rwlock);

    char *buffer = (char*)malloc((int)inode->i_size * sizeof(char));
    int fhandle = add_to_open_file_table(inumb, 0);
    tfs_read(fhandle, buffer, (int)inode->i_size * sizeof(char));

    // Open destination file
    FILE *file = fopen(dest_path, "w");
    if(file == NULL) {
        pthread_rwlock_unlock(&inode->rwlock);
        return -1;
    }

    fprintf(file, buffer, (int)inode->i_size * sizeof(char));

    remove_from_open_file_table(fhandle);
    fclose(file);
    free(buffer);
    pthread_rwlock_unlock(&inode->rwlock);
    return 0;
}