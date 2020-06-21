CC   = gcc
OPTS = -Wall

all: server my_client libmfs

my_server:
	$(CC) server_mfs.c my_server.c udp.c bitarray.c -g -Wall -o server

my_client:
	$(CC) client.c mfs.c udp.c -g -Wall -o client

# this generates the target executables
server: server.o udp.o
	# $(CC) -o server server.o udp.o 
	$(CC) server_mfs.c server.c udp.c bitarray.c -g -Wall -o server

client: client.o udp.o
	# $(CC) -o client client.o udp.o 
	$(CC) -g -Wall -o client mfs.c client.o udp.o

libmfs:
	gcc -shared -o libmfs.so -fPIC mfs.c

# this is a generic rule for .o files 
%.o: %.c 
	$(CC) $(OPTS) -c $< -o $@

clean:
	rm -f server.o udp.o client.o server client

clean_mfs:
	rm -f *.mfsi