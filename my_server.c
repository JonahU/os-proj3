#include <stdio.h>
#include "udp.h"
#include "server_mfs.h"

#define BUFFER_SIZE (4096)

void print_stat(int inum, MFS_Stat_t* stat) {
  printf("inode %d: type=%d, size=%d, blocks=%d\n", inum, stat->type, stat->size, stat->blocks);
}

int find_inum_of_name(FSImage* my_fsi, char* name) {
  int parent_dir_inum = 0;
  int found_inum = -1;
  bool found = false;
  while(!found && parent_dir_inum < INODE_TABLE_SIZE) {
    found_inum = SMFS_lookup(my_fsi, parent_dir_inum, name);
    if(found_inum > 0) {
      found = true;
      printf("Found: %s, inum=%d\n", name, found_inum);
    }
    ++parent_dir_inum;
  }
  return found_inum;
}

int main(int argc, char *argv[]) {
    if(argc<3)
    {
      printf("Usage: server [server-port-number] [file-system-image]\n");
      exit(1);
    }

    int portid = atoi(argv[1]);
    int sd = UDP_Open(portid); //port # 
    assert(sd > -1);

    char const* file_system_image = argv[2];
    FSImage* my_fsi = SMFS_open_file_system_image(file_system_image);

    SMFS_create_file(my_fsi, 0, I_FILE, "abc");
    SMFS_create_file(my_fsi, 0, I_DIRECTORY, "def");
    SMFS_create_file(my_fsi, 0, I_DIRECTORY, "def");

    char w_buf[4096] = { 1, 2, 3, [4094] = 95, [4095] = 96 };
    SMFS_write_block(my_fsi, 1, w_buf, 5);
    SMFS_write_block(my_fsi, 1, w_buf, 6);

    char r_buf[4096] = { 0 };
    SMFS_read_block(my_fsi, 1, r_buf, 5);

    MFS_Stat_t stat = { 0 };
    if (SMFS_stat(my_fsi, 0, &stat) > -1)
      print_stat(1, &stat);

    if (SMFS_stat(my_fsi, 1, &stat) > -1)
      print_stat(2, &stat);

    if (SMFS_stat(my_fsi, 2, &stat) > -1)
      print_stat(3, &stat);

    if (SMFS_stat(my_fsi, 50, &stat) > -1)
      print_stat(50, &stat);
    
    if (SMFS_stat(my_fsi, 0, &stat) > -1)
      print_stat(0, &stat);

    SMFS_unlink(my_fsi, 100, "BAD INUM");
    SMFS_unlink(my_fsi, -1, "BAD INUM2");
    SMFS_unlink(my_fsi, 0, "BAD FILENAME");
    SMFS_unlink(my_fsi, 0, "abc");
    SMFS_unlink(my_fsi, 0, "def");

    SMFS_create_file(my_fsi, 0, I_DIRECTORY, "new+empty+dir");
    SMFS_unlink(my_fsi, 0, "new+empty+dir");

    SMFS_create_file(my_fsi, 0, I_DIRECTORY, "def");
    int def_inum = find_inum_of_name(my_fsi, "def");
    SMFS_create_file(my_fsi, def_inum, I_FILE, "newfile");
    SMFS_unlink(my_fsi, 0, "def"); // trigger NOT empty error
    if (SMFS_stat(my_fsi, 0, &stat) > -1)
      print_stat(1, &stat);

    printf("waiting in loop\n");

    while (1) {
	    struct sockaddr_in s;
	    char buffer[BUFFER_SIZE];
	    int rc = UDP_Read(sd, &s, buffer, BUFFER_SIZE); //read message buffer from port sd
	    if (rc > 0) {
	        printf("SERVER:: read %d bytes (message: '%s')\n", rc, buffer);
	        char reply[BUFFER_SIZE];
	        sprintf(reply, "reply");
	        rc = UDP_Write(sd, &s, reply, BUFFER_SIZE); //write message buffer to port sd
	    }
    }
    return 0;
}
