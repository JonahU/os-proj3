#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include "mfs.h"
#include "udp.h"

char server_name[100] = {0};
int server_port = -1;
int const client_port = 12345;
MFS_ClientToServer request = {0};
MFS_ServerToClient response = {0};

// UDP stuff
int fd = -1;
struct sockaddr_in addr, addr2;

static int wait_timeout() {
    fd_set readfds;
    struct timeval timeout;
    timeout.tv_sec =5; timeout.tv_usec = 0;

    FD_ZERO(&readfds);
    FD_SET(fd, &readfds);

    int ready = select(fd+1, &readfds, NULL, NULL, &timeout);
    // printf("ready=%d\n", ready);
    return ready;
}

/*
MFS_Init() takes a host name and port number and uses those to find the server exporting the file system.
*/
int MFS_Init(char *hostname, int port) {
    strcpy(server_name, hostname);
    server_port = port;

    fd = UDP_Open(client_port); //communicate through specified port 
    assert(fd > -1);

    int rc = UDP_FillSockAddr(&addr, server_name, server_port); //contact server at specified port
    assert(rc == 0);
    printf("MFSInit: %s:%d, fd = %d\n", server_name, server_port, fd);
    return 0;
}

/*
MFS_Lookup() takes the parent inode number (which should be the inode number of a directory) and looks up the entry name in it.
The inode number of name is returned. Success: return inode number of name; failure: return -1. Failure modes: invalid pinum, name does not exist in pinum.
*/
int MFS_Lookup(int pinum, char *name) {
    memset(&request, 0, sizeof request);
    memset(&response, 0, sizeof response);
    strcpy(request.cmd, "MFS_Lookup");
    request.inum = pinum;
    strcpy(request.filename, name);

    int ready = -1;
    while (ready<1) {
        int writebytes = UDP_Write(fd, &addr, (char*)&request, sizeof request); //write message to server@specified-port
        printf("CLIENT:: sent (%s) message (%d)\n", request.cmd, writebytes);

        ready = wait_timeout();
        if (ready < 1) {
            printf("5 second timeout, trying again...\n");
            continue;
        }
    }

    int readbytes = UDP_Read(fd, &addr2, (char*)&response, sizeof response); //read message from ...
	printf("CLIENT:: read %d bytes (message: '%s')\n", readbytes, response.buffer);
    return response.return_val;
}

/*
MFS_Stat() returns some information about the file specified by inum. Upon success, return 0, otherwise -1.
The exact info returned is defined by MFS_Stat_t. Failure modes: inum does not exist.
*/
int MFS_Stat(int inum, MFS_Stat_t *m) {
    memset(&request, 0, sizeof request);
    memset(&response, 0, sizeof response);
    strcpy(request.cmd, "MFS_Stat");
    request.inum = inum;

    int ready = -1;
    while (ready<1) {
        int writebytes = UDP_Write(fd, &addr, (char*)&request, sizeof request); //write message to server@specified-port
        printf("CLIENT:: sent (%s) message (%d)\n", request.cmd, writebytes);

        ready = wait_timeout();
        if (ready < 1) {
            printf("5 second timeout, trying again...\n");
            continue;
        }
    }

    int readbytes = UDP_Read(fd, &addr2, (char*)&response, sizeof response); //read message from ...
	printf("CLIENT:: read %d bytes (message: '%s')\n", readbytes, response.buffer);
    memcpy(m, &response.stat, sizeof *m);
    return response.return_val;
}

/*
MFS_Write() writes a block of size 4096 bytes at the block offset specified by block . Returns 0 on success, -1 on failure.
Failure modes: invalid inum, invalid block, not a regular file (you can't write to directories).
*/
int MFS_Write(int inum, char *buffer, int block) {
    memset(&request, 0, sizeof request);
    memset(&response, 0, sizeof response);
    strcpy(request.cmd, "MFS_Write");
    request.inum = inum;
    request.block = block;
    memcpy(request.buffer, buffer, MFS_BLOCK_SIZE);

    int ready = -1;
    while (ready<1) {
        int writebytes = UDP_Write(fd, &addr, (char*)&request, sizeof request); //write message to server@specified-port
        printf("CLIENT:: sent (%s) message (%d)\n", request.cmd, writebytes);

        ready = wait_timeout();
        if (ready < 1) {
            printf("5 second timeout, trying again...\n");
            continue;
        }
    }

    int readbytes = UDP_Read(fd, &addr2, (char*)&response, sizeof response); //read message from ...
	printf("CLIENT:: read %d bytes (message: '%s')\n", readbytes, response.buffer);
    return response.return_val;
}

/*
MFS_Read() reads a block specified by block into the buffer from file specified by inum .
The routine should work for either a file or directory; directories should return data in the format specified by MFS_DirEnt_t.
Success: 0, failure: -1. Failure modes: invalid inum, invalid block.
*/
int MFS_Read(int inum, char *buffer, int block) {
    memset(&request, 0, sizeof request);
    memset(&response, 0, sizeof response);
    strcpy(request.cmd, "MFS_Read");
    request.inum = inum;
    request.block = block;
    memcpy(request.buffer, buffer, MFS_BLOCK_SIZE);

    int ready = -1;
    while (ready<1) {
        int writebytes = UDP_Write(fd, &addr, (char*)&request, sizeof request); //write message to server@specified-port
        printf("CLIENT:: sent (%s) message (%d)\n", request.cmd, writebytes);

        ready = wait_timeout();
        if (ready < 1) {
            printf("5 second timeout, trying again...\n");
            continue;
        }
    }

    int readbytes = UDP_Read(fd, &addr2, (char*)&response, sizeof response); //read message from ...
    memcpy(buffer, response.buffer, MFS_BLOCK_SIZE);
	printf("CLIENT:: read %d bytes (message: '%s')\n", readbytes, response.buffer);
    return response.return_val;
}

/*
MFS_Creat() makes a file ( type == MFS_REGULAR_FILE) or directory ( type == MFS_DIRECTORY) in the parent directory specified by pinum of name name .
Returns 0 on success, -1 on failure. Failure modes: pinum does not exist. If name already exists, return success (think about why).
*/
int MFS_Creat(int pinum, int type, char *name) {
    memset(&request, 0, sizeof request);
    memset(&response, 0, sizeof response);
    strcpy(request.cmd, "MFS_Creat");
    request.inum = pinum;
    request.filetype = type;
    strcpy(request.filename, name);

    int ready = -1;
    while (ready<1) {
        int writebytes = UDP_Write(fd, &addr, (char*)&request, sizeof request); //write message to server@specified-port
        printf("CLIENT:: sent (%s) message (%d)\n", request.cmd, writebytes);

        ready = wait_timeout();
        if (ready < 1) {
            printf("5 second timeout, trying again...\n");
            continue;
        }
    }

    int readbytes = UDP_Read(fd, &addr2, (char*)&response, sizeof response); //read message from ...
	printf("CLIENT:: read %d bytes\n", readbytes);
    return response.return_val;
}


/*
MFS_Unlink() removes the file or directory name from the directory specified by pinum .
0 on success, -1 on failure. Failure modes: pinum does not exist, pinum does not represent a directory, the to-be-unlinked directory is NOT empty.
Note that the name not existing is NOT a failure by our definition (think about why this might be).
*/
int MFS_Unlink(int pinum, char *name) {
    memset(&request, 0, sizeof request);
    memset(&response, 0, sizeof response);
    strcpy(request.cmd, "MFS_Unlink");
    request.inum = pinum;
    strcpy(request.filename, name);

    int ready = -1;
    while (ready<1) {
        int writebytes = UDP_Write(fd, &addr, (char*)&request, sizeof request); //write message to server@specified-port
        printf("CLIENT:: sent (%s) message (%d)\n", request.cmd, writebytes);

        ready = wait_timeout();
        if (ready < 1) {
            printf("5 second timeout, trying again...\n");
            continue;
        }
    }

    int readbytes = UDP_Read(fd, &addr2, (char*)&response, sizeof response); //read message from ...
	printf("CLIENT:: read %d bytes (message: '%s')\n", readbytes, response.buffer);
    return response.return_val;
}
