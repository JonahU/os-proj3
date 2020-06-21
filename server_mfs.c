#include <assert.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "server_mfs.h"

static int empty_block_index(FSImage* my_fsi) {
    int i = 0;
    while (test_bit(my_fsi->mfs->block_alloc, i)) {
        ++i;
    }
    set_bit(my_fsi->mfs->block_alloc, i); // should this be done automatically here?
    return i;   
}

static int empty_inode_index(FSImage* my_fsi) {
    int i = 1; // inode 0 is root directory inode
    while (test_bit(my_fsi->mfs->inode_alloc, i)) {
        ++i;
    }
    set_bit(my_fsi->mfs->inode_alloc, i); // should this be done automatically here?
    return i;    
}

static ssize_t get_dir_size(dir_file* dir) {
    ssize_t d_count_size = 0; // sizeof dir->d_count; // commenting this out because tests dont expect an extra field
    ssize_t d_entries_size = dir->d_count * sizeof dir->d_entries[0];
    return d_count_size + d_entries_size;
}

static int get_dir_entry_count(FSImage* my_fsi, int inum) {
    inode* inode = &my_fsi->mfs->inode_table[inum];
    int dir_entry_count = 0;
    for(int i=0; i<BLOCK_PTRS; i++) {
        dir_file* dir = &inode->block_ptrs[i]->b_directory;
        if (dir != NULL) {
            dir_entry_count += dir->d_count;
        }
    }
    return dir_entry_count;
}

static bool is_dir_empty(FSImage* my_fsi, int inum) {
    return get_dir_entry_count(my_fsi, inum) == 2;
}

static int init_directory(FSImage* my_fsi, int inum, int pinum) {
    // create new directory
    dir_file new_dir = {
        .d_entries = {
            { .d_name = "." , .inode_num = inum  },
            { .d_name = "..", .inode_num = pinum }
        },
        .d_count  = 2
    };

    // find empty block and copy directory into it
    int blk_index = empty_block_index(my_fsi);
    block* dest = &my_fsi->mfs->data_blocks[blk_index];
    memcpy(dest, &new_dir, sizeof new_dir);

    // update inode
    inode* my_inode = &my_fsi->mfs->inode_table[inum];
    my_inode->type = I_DIRECTORY;
    my_inode->size = get_dir_size(&new_dir);
    my_inode->block_alloc_count = 1;
    my_inode->block_ptrs[0] = dest;
    return 0;
}

static void force_to_disk(FSImage* my_fsi) {
    // write file system image to disk
    char* mfs_ptr = (char*)(my_fsi->mfs);
    size_t left_to_write = sizeof *(my_fsi->mfs);
    while (left_to_write > 0) {
        ssize_t written = write(my_fsi->fd, mfs_ptr, left_to_write);
        assert(written > -1);
        mfs_ptr += written;
        left_to_write -= written;
    }
    assert(left_to_write == 0);
    lseek(my_fsi->fd, 0, SEEK_SET); // set file offset to point to beginning of file
    fsync(my_fsi->fd); // force to disk
}

static int inode_get_free_block(FSImage* my_fsi, inode* in, bool* new_block) {
    if (new_block)
        *new_block = false;

    if (in->type == I_DIRECTORY) {
        // first check if any occupied blocks have space
        for (int i=0; i< BLOCK_PTRS; i++) {
            block* blkptr = in->block_ptrs[i];
            if(blkptr != NULL && blkptr->b_directory.d_count != DENTRIES_MAX) {
                return i;
            }
        }
        // check for empty blocks
        for (int i=0; i< BLOCK_PTRS; i++) {
            if(in->block_ptrs[i] == NULL) {
                if (new_block)
                    *new_block = true;
                return i;
            }

        }
    } else if (in->type == I_FILE) {
        // check for empty blocks
        for (int i=0; i< BLOCK_PTRS; i++) {
            if(in->block_ptrs[i] == NULL) {
                if (new_block)
                    *new_block = true;
                return i;
            }
        }
    }

    // no free blocks
    return -1;
}

static bool is_valid_inum(int inum) {
    if (inum > INODE_TABLE_SIZE-1 || inum < 0)
        return false;
    else
        return true;
}

static bool is_valid_blknum(int blknum) {
    if (blknum > BLOCK_COUNT-1 || blknum < 0)
        return false;
    else
        return true;
}

static bool is_valid_blkoffset(int blknum) {
    if (blknum > BLOCK_PTRS-1 || blknum < 0)
        return false;
    else
        return true;
}

static bool is_valid_file_type(FSImage* my_fsi, int inum, i_type type) {
    inode* my_inode = &my_fsi->mfs->inode_table[inum];
    if (my_inode->type != type)
        return false;
    else
        return true;
}

static int dir_find_inode(FSImage* my_fsi, int pinum, char const* name) {
    inode* parent_inode = &my_fsi->mfs->inode_table[pinum];
    for(int i=0; i<BLOCK_PTRS; i++) {
        dir_file* dir = &parent_inode->block_ptrs[i]->b_directory;
        if (dir != NULL) {
            for(int j=0; j<dir->d_count; j++) {
                dir_file_entry* entry = &dir->d_entries[j];
                if (strcmp(entry->d_name, name) == 0) {
                    int found_inum = entry->inode_num;
                    return found_inum;
                }
            }
        }
    }
    return -1; // 0 = invalid inode
}

static bool is_valid_file_name(FSImage* my_fsi, int pinum, char const* name) {
    return dir_find_inode(my_fsi, pinum, name) == -1 ? false : true;
}

static dir_file_entry* add_dir_entry(dir_file* dir, int inum, char const* filename) {
    // is space in current dir file ?
    if (dir->d_count < DENTRIES_MAX) {
        dir_file_entry* new_entry = &dir->d_entries[dir->d_count];
        new_entry->inode_num = inum;
        strcpy(new_entry->d_name, filename);
        ++(dir->d_count); // update directory count
        return new_entry;
    }
    printf("dir count = %d\n",dir->d_count);
    fprintf(stderr, "ERROR: add_dir_entry() failure. THIS SHOULD NEVER HAPPEN, SOMETHING IS WRONG\n");
    return NULL;

}

static dir_file* find_dir_file(FSImage* my_fsi, int inum, char const* filename) {
    dir_file* found = NULL;
    inode* inode = &my_fsi->mfs->inode_table[inum];

    for(int i=0; i<BLOCK_PTRS; i++) {
        dir_file* found = &inode->block_ptrs[i]->b_directory;
        if (found != NULL) {
            for(int j=0; j<found->d_count; j++) {
                dir_file_entry* entry = &found->d_entries[j];
                if (strcmp(entry->d_name, filename) == 0) {
                    return found;
                }
            }
        }
    }
    return found;
}

static int remove_dir_entry(dir_file* dir, char const*filename) {
    // find entry
    dir_file_entry* found = NULL;
    int index = 0;
    for (index=0; index<dir->d_count; index++) {
        if(strcmp(dir->d_entries[index].d_name, filename) == 0) {
            found = &dir->d_entries[index];
            break;
        }
        ++found;
    }
    if(found == NULL)
        return -1;

    int deleted_entry_inum = found->inode_num;
    
    // if file is last entry in dir file
    if (index == (dir->d_count)-1) {
        memset(found, 0, sizeof *found); // reset file entry
        --(dir->d_count);
        return deleted_entry_inum;
    }

    // copy last element into the deleted dir entry's location
    memcpy(found, &dir->d_entries[dir->d_count -1] , sizeof *found);
    memset(&dir->d_entries[dir->d_count -1], 0, sizeof *found);
    --(dir->d_count);
    return deleted_entry_inum;
}

static void update_inode(inode* in, unsigned size, unsigned blocks_allocated) {
    (in->size) += size;

    (in->block_alloc_count) += blocks_allocated;
    // Note: if block allocated, this function doesn't deal with updating inode->block_ptrs with that new block
}

static int remove_block_from_bitarray(FSImage* my_fsi, block* block_ptr) {
    for(int i=0; i<BLOCK_COUNT; i++) {
        if (block_ptr == &my_fsi->mfs->data_blocks[i]) {
            clear_bit(my_fsi->mfs->block_alloc, i);
            return 0;
        }
    }
    return -1;
}

/*
Initialize file system image to include an empty root directory with . and .. entries.
Create space big enough for inode table and 4096 data blocks.
*/
int SMFS_init_file_system_image(FSImage* my_fsi) {
    // create file system image
    SMFS* my_file_system = calloc(1, sizeof *my_file_system);
    my_fsi->mfs = my_file_system;

    // init root directory
    int root_inum = 0;
    init_directory(my_fsi, root_inum, root_inum); // inum + parent inum are the same for root dir
    set_bit(my_file_system->inode_alloc, root_inum); // update allocated inode bitarray

    // write file system image to disk
    force_to_disk(my_fsi);
    return 0;
}

/*
Open file system image if it exists then return file descriptor.
If file system image doesn't exist, will create a new file and call SMFS_init_file_system_image.
*/
FSImage* SMFS_open_file_system_image(char const* fsi) {
    FSImage* my_fsi = malloc(sizeof *my_fsi);
    char fsi_filename[strlen(fsi) + 6]; // ".mfsi" extension + '\0'
    strcpy(fsi_filename, fsi);
    strcat(fsi_filename, ".mfsi");
    int fd = open(fsi_filename, O_RDWR);
    if (fd < 0 && errno == ENOENT) {
        // file does not exist, create it
        printf("SERVER:: creating new file system image '%s'\n", fsi_filename);
        fd = open(fsi_filename, O_RDWR | O_CREAT, S_IRWXU);
        assert(fd > -1);
        my_fsi->fd = fd;
        SMFS_init_file_system_image(my_fsi);
    } else {
        printf("SERVER:: opening existing file system image '%s'\n", fsi_filename);
        // get size of file + malloc buffer of that size
        struct stat statbuf;
        fstat(fd, &statbuf);
        char* readbuf = malloc(statbuf.st_size);
        
        // read file contents into buffer
        char* read_ptr = readbuf;
        ssize_t left_to_read = statbuf.st_size;
        while (left_to_read > 0) {
            ssize_t bytes_read = read(fd, read_ptr, left_to_read);
            read_ptr += bytes_read;
            left_to_read -= bytes_read;
        }
        assert(left_to_read == 0);

        // init my_fsi
        my_fsi->fd = fd;
        my_fsi->mfs = (SMFS*)readbuf;
    }
    return my_fsi;
}

// what about closing the file system image?

/*
takes the parent inode number (which should be the inode number of a directory) and looks up the entry name in it.
The inode number of name is returned. 
Success: return inode number of name;
failure: return -1. Failure modes: invalid pinum, name does not exist in pinum.
*/
int SMFS_lookup(FSImage* my_fsi, int pinum, char* name) {
    if (!is_valid_inum(pinum)) {
        fprintf(stderr, "ERROR: (SMFS_lookup) invalid parent inum '%d'\n", pinum);
        return -1;
    } else if (!is_valid_file_type(my_fsi, pinum, I_DIRECTORY)) {
        fprintf(stderr, "ERROR: (SMFS_lookup) parent inum '%d' is not a directory\n", pinum);
        return -1;
    } else if (!is_valid_file_name(my_fsi, pinum, name)) {
        fprintf(stderr, "ERROR: (SMFS_lookup) filename '%s' is not in parent inum '%d'\n", name, pinum);
        return -1;
    }
    return dir_find_inode(my_fsi, pinum, name);
}

int SMFS_create_file(FSImage* my_fsi, int pinum, i_type type, char const* filename) {
    if (type == I_EMPTY || pinum < 0 || my_fsi == NULL || strlen(filename) == 0) {
        fprintf(stderr, "ERROR: (SMFS_create_file) invalid input\n");
        return -1;
    }
    
    inode* parent_inode = &my_fsi->mfs->inode_table[pinum];
    if(parent_inode->type != I_DIRECTORY) {
        fprintf(stderr, "ERROR: (SMFS_create_file) inode[pinum=%d] is not a directory\n", pinum);
        return -1;
    }

    if(is_valid_file_name(my_fsi, pinum, filename)) {
        // "If name already exists, return success (think about why)."
        printf("SERVER::SMFS_create_file file '%s' already exists\n", filename);
        return 0;
    }

    // get new inode
    int new_inode_index = empty_inode_index(my_fsi);

    // find space to put new directory entry
    bool new_block_required = false;
    int blkptr = inode_get_free_block(my_fsi, parent_inode, &new_block_required);
    if (blkptr < 0) {
        fprintf(stderr, "ERROR: (SMFS_create_file) directory file is out of space\n");
        return -1;
    }

    if (new_block_required) {
        int new_blk_index = empty_block_index(my_fsi);
        parent_inode->block_ptrs[blkptr] = &my_fsi->mfs->data_blocks[new_blk_index];
    }
    dir_file* dir = &parent_inode->block_ptrs[blkptr]->b_directory;
    
    // create new directory entry + update parent inode
    dir_file_entry* new_entry = add_dir_entry(dir, new_inode_index, filename);
    update_inode(parent_inode, sizeof *new_entry, new_block_required ? 1 : 0);
    
    // create new file if necessary + init new inode
    if(type == I_DIRECTORY) {
        init_directory(my_fsi, new_inode_index, pinum);
    } else if (type == I_FILE) {
        inode* new_inode = &my_fsi->mfs->inode_table[new_inode_index];
        new_inode->type = I_FILE;
        new_inode->size = 0;
        new_inode->block_alloc_count = 0;
    }

    // write updates to disk
    force_to_disk(my_fsi);
    return 0;
}

int SMFS_read_block(FSImage* my_fsi, int inum, char* buffer, int blkoffset) {
    if (
        !is_valid_inum(inum) ||
        !is_valid_blkoffset(blkoffset) ||
        is_valid_file_type(my_fsi, inum, I_EMPTY) // cannot read empty block
    ) {
        fprintf(stderr, "ERROR: (SMFS_read_block) invalid input\n");
        return -1;
    }

    inode* inode = &my_fsi->mfs->inode_table[inum];
    if (blkoffset >= inode->block_alloc_count) {
        fprintf(stderr, "ERROR: (SMFS_read_block) blkoffset >= inode->block_alloc_count\n");
        return -1;
    }

    // copy block to buffer
    block* src = inode->block_ptrs[blkoffset];
    inode->type == I_DIRECTORY ?
        (memcpy(buffer, src, get_dir_size(&src->b_directory))) :
        (memcpy(buffer, src, BLOCK_SIZE));

    return 0;
}

int SMFS_write_block(FSImage* my_fsi, int inum, char* buffer, int blkoffset) {
    if (
        !is_valid_inum(inum) ||
        !is_valid_blknum(blkoffset) ||
        !is_valid_file_type(my_fsi, inum, I_FILE) // cannot write to directory
    ) {
        fprintf(stderr, "ERROR: (SMFS_write_block) invalid input\n");
        return -1;
    }

    // write block
    block* dest = &my_fsi->mfs->data_blocks[blkoffset];
    memcpy(dest, buffer, BLOCK_SIZE);

    // update inode
    inode* my_inode = &my_fsi->mfs->inode_table[inum];
    update_inode(my_inode, BLOCK_SIZE, 1);
    my_inode->block_ptrs[(my_inode->block_alloc_count)-1] = dest; // unlink implementation reorders block ptrs if necessary, otherwise this won't work
    
    // write updates to disk
    force_to_disk(my_fsi);
    return 0;
}

/*
returns some information about the file specified by inum. Upon success, return 0, otherwise -1.
The exact info returned is defined by MFS_Stat_t. 
Failure modes: inum does not exist.
*/
int SMFS_stat(FSImage* my_fsi, int inum, MFS_Stat_t* stat) {
    if (
        !is_valid_inum(inum) ||
        is_valid_file_type(my_fsi, inum, I_EMPTY) // inum is empty i.e. doesn't exist
    ) {
        fprintf(stderr, "ERROR: (SMFS_stat) invalid inum\n");
        return -1;
    }

    inode* my_inode = &my_fsi->mfs->inode_table[inum];
    
    my_inode->type == I_DIRECTORY ?
        (stat->type = MFS_DIRECTORY) :
        (stat->type = MFS_REGULAR_FILE);
    stat->size = my_inode->size;
    stat->blocks = my_inode->block_alloc_count;
    return 0;
}

/*
removes the file or directory name from the directory specified by pinum .
0 on success, -1 on failure.
Failure modes:
    -pinum does not exist
    -pinum does not represent a directory
    -the to-be-unlinked directory is NOT empty.
Note that the name not existing is NOT a failure by our definition (think about why this might be).
*/
int SMFS_unlink(FSImage* my_fsi, int pinum, char* filename) {
    if (!is_valid_inum(pinum) || is_valid_file_type(my_fsi, pinum, I_EMPTY)) {
        fprintf(stderr, "ERROR: (SMFS_unlink) pinum[%d] does not exist\n", pinum);
        return -1;
    } else if(!is_valid_file_type(my_fsi, pinum, I_DIRECTORY)) {
        fprintf(stderr, "ERROR: (SMFS_unlink) pinum[%d] is not a directory\n", pinum);
        return -1;
    }

    if (!is_valid_file_name(my_fsi, pinum, filename)) {
        // Note that the name not existing is NOT a failure by our definition (think about why this might be).
        printf("SERVER::SMFS_unlink file '%s' does not exist in directory with pinum[%d]\n", filename, pinum);
        return 0;
    }

    int inum = dir_find_inode(my_fsi, pinum, filename);
    if(is_valid_file_type(my_fsi, inum, I_DIRECTORY) && !is_dir_empty(my_fsi, inum)) {
        fprintf(stderr, "ERROR: (SMFS_unlink) to-be-unlinked directory file '%s' is NOT empty\n", filename);
        return -1;
    }

    printf("SERVER::SMFS_unlink unlinking file '%s' from pinum[%d]\n", filename, pinum);

    dir_file* dir = find_dir_file(my_fsi, pinum, filename);
    
    // delete dir_entry from file & reorder dir file if necessary
    int remove_inum = remove_dir_entry(dir, filename);

    inode* remove_inode = &my_fsi->mfs->inode_table[remove_inum];

    // remove the file block(s) & remove block(s) from allocated block bitarray
    for(int i = 0; i<remove_inode->block_alloc_count; i++) {
        block* block_ptr = remove_inode->block_ptrs[i];
        remove_block_from_bitarray(my_fsi, block_ptr);
        memset(block_ptr, 0, BLOCK_SIZE);
    }

    // remove its inode from inode table
    memset(remove_inode, 0, sizeof *remove_inode);
    // remove inode from alloc inode bitarray
    clear_bit(my_fsi->mfs->inode_alloc, remove_inum);

    // update parent inode size
    inode* parent_inode = &my_fsi->mfs->inode_table[pinum];
    parent_inode->size -= sizeof(dir_file_entry);

    // check if parent directory block is now empty
    if(dir->d_count == 0) {
        block* dir_block = (block*)dir;

        // remove allocated block from bitarray
        remove_block_from_bitarray(my_fsi, dir_block);

        // remove dir from pinum block ptrs + reorder
        for(int i = 0; i<parent_inode->block_alloc_count; i++) {
            if (parent_inode->block_ptrs[i] == dir_block) {
                if (i == parent_inode->block_alloc_count - 1) {
                    parent_inode->block_ptrs[i] = NULL;
                } else {
                    block* last_block_ptr = parent_inode->block_ptrs[parent_inode->block_alloc_count -1];
                    parent_inode->block_ptrs[i] = last_block_ptr;
                    parent_inode->block_ptrs[parent_inode->block_alloc_count -1] = NULL;
                }
                break;
            }
        }

        // memset 0 the empty dir block
        memset(dir_block, 0, sizeof *dir_block);
    }

    // write updates to disk
    force_to_disk(my_fsi);
    return 0;
}

int SMFS_exec(FSImage* my_fsi, MFS_ClientToServer* request, MFS_ServerToClient* response) {
    char* cmd = request->cmd;

    int inum = request->inum;
    i_type inode_type = request->filetype == MFS_DIRECTORY ? I_DIRECTORY : I_FILE;
    char* filename = request->filename;
    char buf[BLOCK_SIZE];
    memcpy(buf, request->buffer, BLOCK_SIZE);
    MFS_Stat_t stat= {0};
    int blkoffset = request->block;

    int returncode = -1;
    if (strcmp(cmd, "MFS_Creat") == 0) {
        returncode = SMFS_create_file(my_fsi, inum, inode_type, filename);
    } else if (strcmp(cmd, "MFS_Lookup") == 0) {
        returncode = SMFS_lookup(my_fsi, inum, filename);

    } else if (strcmp(cmd, "MFS_Stat") == 0) {
        returncode = SMFS_stat(my_fsi, inum, &stat);

    } else if (strcmp(cmd, "MFS_Write") == 0) {
        returncode = SMFS_write_block(my_fsi, inum, buf, blkoffset);
    } else if (strcmp(cmd, "MFS_Read") == 0) {
        returncode = SMFS_read_block(my_fsi, inum, buf, blkoffset);

    } else if (strcmp(cmd, "MFS_Unlink") == 0) {
        returncode = SMFS_unlink(my_fsi, inum, filename);
    } else {
        printf("TODO: NOT IMPLEMENTED CMD %s\n", cmd);
        return -1;
    }

    memcpy(&response->stat, &stat, sizeof stat);
    memcpy(response->buffer, buf, BLOCK_SIZE);
    response->return_val = returncode;
    return 0;
}

