#include <netinet/in.h>
#include <stdio.h>
#include <sys/select.h>
#include "mfs.h"
#include "udp.h"

// #define BUFFER_SIZE (4096)
// char buffer[BUFFER_SIZE];

int timeout(int fd, char* server_name, char* server_port) {
  // deal with the case where the server does not reply in a timely fashion;
  // the way it deals with that is simply by retrying the operation, after a timeout of some kind (default: five second timeout).

  struct sockaddr_in addr, addr2;
  int rc = UDP_FillSockAddr(&addr, server_name, atoi(server_port)); //contact server at specified port
  assert(rc == 0);


  while(1) {
    fd_set readfds;
    struct timeval timeout;
    timeout.tv_sec =5; timeout.tv_usec = 0;

    // char message[BUFFER_SIZE];
    // sprintf(message, "hello world");
    MFS_ClientToServer request = { .cmd = "TESTCMD"};


    int writebytes = UDP_Write(fd, &addr, (char*)&request, sizeof request); //write message to server@specified-port
    printf("CLIENT:: sent message (%d)\n", writebytes);


    FD_ZERO(&readfds);
    for (int i =0; i <=fd; i++) {
      FD_SET(i, &readfds);
    }
    
    printf("rc=%d\n", writebytes);

    int ready = select(fd+1, &readfds, NULL, NULL, &timeout);
    printf("ready=%d\n", ready);
    
    if (ready < 1) {
      printf("5 second timeout, trying again...\n");
      continue;
    }

    MFS_ServerToClient response;
	  int readbytes = UDP_Read(fd, &addr2, (char*)&response, sizeof response); //read message from ...
	  printf("CLIENT:: read %d bytes (message: '%s')\n", readbytes, response.buffer);

    
  }
}


int
main(int argc, char *argv[])
{
    if(argc<4)
    {
      printf("Usage: client [server-name] [server-port] [client-port]\n");
      exit(1);
    }
    char* server_name = argv[1];
    char* server_port = argv[2];
    // char* client_port = argv[3];

// START EXEC TEST
  MFS_Init(server_name, atoi(server_port));

  int rc = MFS_Lookup(0, ".");
  if (MFS_Lookup(0, ".") != 0) {
    printf("FAILED");
    return -1;

  }
  printf("PASSED");
  
// END EXEC TEST
    return 0;
}