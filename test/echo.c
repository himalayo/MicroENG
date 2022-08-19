#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <sched.h>
#define BUFSIZE 1024*1024*5

typedef struct shared_header {
	char bitmask;
	int argument;
	size_t len;
}Head;

void main(int argc, char** argv) {

        int fd = shm_open("/testmicroengreads", O_RDWR,0);
        char *mapped_mem = mmap(NULL,BUFSIZE,
            PROT_READ|PROT_WRITE,MAP_SHARED,
            fd,0);
	mapped_mem += sizeof(Head);
	Head* header = mmap(NULL,sizeof(Head),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
	close(fd);	
	
	int echo_fd = shm_open("/testmicroengwrites", O_RDWR,0);

        char *write_into = mmap(NULL,BUFSIZE,
            PROT_READ|PROT_WRITE,MAP_SHARED,
            echo_fd,0);
	Head* write_header = mmap(NULL,sizeof(Head),PROT_READ|PROT_WRITE,MAP_SHARED,echo_fd,0);
	close(echo_fd);
	write_into += sizeof(Head);
	
	if (header->bitmask == 1<<2 )
	{
		write(STDOUT_FILENO,mapped_mem,header->len);
	}

	header->bitmask = 1;
	do
	{
	}while(header->bitmask == 1);
	
	write(STDOUT_FILENO,mapped_mem,header->len);
	write_header->argument = header->argument;
	write_header->len = header->len;

	memcpy(write_into,mapped_mem,header->len);

	write_header->bitmask = 1; // HEADER_SET_PACKET	
}
