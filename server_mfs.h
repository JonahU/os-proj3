#pragma once

#include <stdint.h>
#include "bitarray.h"
#include "mfs.h"

#define INODE_TABLE_SIZE 4096
#define BLOCK_COUNT      4096
#define BLOCK_SIZE       4096
#define BLOCK_PTRS       10
#define DNAME_MAX        252
#define DENTRIES_MAX     16     // (blocksize - d_count - reserved) [4094 bytes] / dir entry size [254 bytes]

typedef struct dir_file_entry_ {
    int     inode_num;
    char    d_name[DNAME_MAX];
} dir_file_entry;

typedef struct dir_file_ {
    dir_file_entry d_entries[DENTRIES_MAX];
    int8_t         d_count;
    int8_t         reserved;
    char           padding[30]; // 16*254 + 1 + 1 + 30 = 4096
} dir_file;

typedef struct file_file_ {
    char f_data[BLOCK_SIZE];
} file_file;

typedef union block_ {
    file_file b_file;
    dir_file  b_directory;
} block;

typedef enum { I_EMPTY, I_DIRECTORY, I_FILE } i_type;

typedef struct inode_ {
    unsigned size;
    unsigned block_alloc_count;
    block*   block_ptrs[BLOCK_PTRS];
    i_type   type;
} inode;

typedef struct SMFS_ {
    bitarray inode_alloc;
    bitarray block_alloc;
    inode inode_table[INODE_TABLE_SIZE];
    block data_blocks[BLOCK_COUNT];
} SMFS;

typedef struct FSImage_ {
    int fd;
    SMFS* mfs;
} FSImage;

FSImage* SMFS_open_file_system_image (char const* fsi);
int      SMFS_init_file_system_image (FSImage* my_fsi);
int      SMFS_exec                   (FSImage* my_fsi, MFS_ClientToServer* request, MFS_ServerToClient* response);

int      SMFS_lookup                 (FSImage* my_fsi, int pinum, char* name);
int      SMFS_create_file            (FSImage* my_fsi, int pinum, i_type type, char const* filename);
int      SMFS_read_block             (FSImage* my_fsi, int inum, char* buffer, int blkoffset);
int      SMFS_write_block            (FSImage* my_fsi, int inum, char* buffer, int blkoffset);
int      SMFS_stat                   (FSImage* my_fsi, int inum, MFS_Stat_t* stat);
int      SMFS_unlink                 (FSImage* my_fsi, int pinum, char* filename);