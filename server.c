#include <stdio.h>
#include "udp.h"
#include "server_mfs.h"

// #define BUFFER_SIZE (4096)

// void print_stat(int inum, MFS_Stat_t* stat) {
//   printf("inode %d: type=%d, size=%d, blocks=%d\n", inum, stat->type, stat->size, stat->blocks);
// }

// int find_inum_of_name(FSImage* my_fsi, char* name) {
//   int parent_dir_inum = 0;
//   int found_inum = -1;
//   bool found = false;
//   while(!found && parent_dir_inum < INODE_TABLE_SIZE) {
//     found_inum = SMFS_lookup(my_fsi, parent_dir_inum, name);
//     if(found_inum > 0) {
//       found = true;
//       printf("Found: %s, inum=%d\n", name, found_inum);
//     }
//     ++parent_dir_inum;
//   }
//   return found_inum;
// }

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

    printf("waiting in loop\n");

    while (1) {
	    struct sockaddr_in s;
      MFS_ClientToServer request = {0};

	    int rc = UDP_Read(sd, &s, (char*)&request, sizeof request); //read message buffer from port sd
	    if (rc > 0) {
	        printf("SERVER:: read %d bytes (cmd: '%s')\n", rc, request.cmd);
          MFS_ServerToClient response = {0};

          int exec_success = SMFS_exec(my_fsi, &request, &response);
          if (exec_success < 0) {
            printf("SERVER:: exec failed (cmd: '%s')\n", request.cmd);
            continue;
          }

	        rc = UDP_Write(sd, &s, (char*)&response, sizeof response); //write message buffer to port sd
	    }
    }
    return 0;
}
