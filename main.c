/*
 *
 *		MicroENG: The server middle-man that absolutely nobody asked for.
 *
 *		Goals:
 *			1) Be fast
 *			2) Be asynchronous
 *			3) Be modular
 *		
 *		License:
 *			* I don't care
 *			* please dont sue me i have a wife and two kids :(
 */

#include <stdio.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#define MAX_LOOP 50
#define SIZE 5*1024*1024

#define HEADER_ERR	  1<<7

#define READ_HEADER_PRINT 1
#define READ_HEADER_SEND  1<<2

#define WRITE_HEADER_GET  1
#define WRITE_HEADER_SET  1<<2

typedef struct header
{
	char bitmask;
	int  argument;
	size_t size;
} SharedHeader;

typedef struct packet
{
	int connection_fd;
	char* bytes;
	size_t size;
} Packet;

typedef struct packet_history
{
	Packet* packets;
	size_t  size;
	int offset;
} PacketHistory;

typedef struct connections {
	int* file_descriptors;
	size_t size;
} Connections;

typedef struct interface_args
{
	void* buffer;
	Connections* connection_list;
	PacketHistory* packet_list;
} InterfaceArguments;

typedef struct connection_poll
{
	struct pollfd* poll_buffer;
	size_t size;
} ConnectionPoll;

/*
 *	Reads from the input buffer, and responds to commands accordingly. The following commands are supported:
 *				*PRINT -> print the buffer into standard output
 *				*SEND  -> send  the buffer to a socket connection as determined by the header's argument
 */
void*
handle_input(void* args)
{
	InterfaceArguments* input = (InterfaceArguments*)args;
	SharedHeader* read_header = (SharedHeader*)input->buffer;
	char* 	      read_data   = (char*)input->buffer+sizeof(SharedHeader);

	for (;;)
	{
		if (read_header->bitmask & READ_HEADER_PRINT)
		{
			write(STDOUT_FILENO,read_data,read_header->size);
			read_header->bitmask = 0;
		}

		if (read_header->bitmask & READ_HEADER_SEND)
		{
			if (read_header->argument < 0 || read_header->argument >= input->connection_list->size)
			{
				read_header->bitmask = HEADER_ERR;
				continue;
			}


			send(
				input->connection_list->file_descriptors[read_header->argument],
				read_data,read_header->size,
				MSG_DONTWAIT
			);

			read_header->bitmask = 0;
		}
	}

	return NULL;
}


/*
 *	Writes into the output buffer, according to requests. The following commands are supported:
 *				*GET -> fetch a packet as determined by the header's argument
 */
void*
handle_output(void* ptr)
{
	InterfaceArguments* input = (InterfaceArguments*)ptr;
	SharedHeader* write_header = (SharedHeader*)input->buffer;
	char*	      write_data    = (char*)input->buffer+sizeof(SharedHeader);

	for (;;)
	{
		if (write_header->bitmask & WRITE_HEADER_GET)
		{
			if (write_header->argument < 0 || write_header->argument >= input->packet_list->size)
			{
				write_header->bitmask = HEADER_ERR;
				continue;
			}

			memcpy(
				write_data,
				input->packet_list->packets[write_header->argument].bytes,
				input->packet_list->packets[write_header->argument].size
			);
			write_header->size = input->packet_list->packets[write_header->argument].size;
			write_header->bitmask = WRITE_HEADER_SET;
		}
	}

	return NULL;
}


/*
 *	Sets up a shared memory address
 */
void*
get_shared_mem(const char* filename,size_t size)
{
	int fd = shm_open(filename,O_RDWR|O_CREAT,S_IRWXU|S_IRWXG|S_IRWXO);
	ftruncate(fd,size);
	void* output = mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
	close(fd);
	return output;
}

/*
 *	Manages program's state
 */
void
main(void)
{
	/*
	 *	Allocate all buffers and the shared memory 
	 */
	void* write_into = get_shared_mem("/microengwrites",SIZE);
	void* read_from  = get_shared_mem("/microengreads",SIZE);

	if (write_into == MAP_FAILED || read_from == MAP_FAILED)
	{
		exit(EXIT_FAILURE);
	}

	Connections* curr_connections = malloc(sizeof(Connections));
	PacketHistory* packet_log = malloc(sizeof(PacketHistory));
	
	curr_connections->file_descriptors = malloc(sizeof(int));
	packet_log->packets = malloc(sizeof(Packet));

	ConnectionPoll* main_poll = malloc(sizeof(ConnectionPoll));
	main_poll->poll_buffer = malloc(sizeof(struct pollfd));

	struct poll_list {
		pthread_t*      thread_list;
		size_t          thread_list_size;
		ConnectionPoll* polls;
		size_t          size;
	} connection_polls;
	
	/*
	 *	Setup threads and their respective required data structures
	 */
	InterfaceArguments* input_subroutine_arguments = malloc(sizeof(InterfaceArguments));
	InterfaceArguments* output_subroutine_arguments = malloc(sizeof(InterfaceArguments));
	
	input_subroutine_arguments->buffer = read_from;
	input_subroutine_arguments->connection_list = curr_connections;
	input_subroutine_arguments->packet_list     = packet_log;

	output_subroutine_arguments->buffer = write_into;
	output_subroutine_arguments->connection_list = curr_connections;
	output_subroutine_arguments->packet_list     = packet_log;
	
	/*
	 *	Create the threads
	 */
	pthread_t writing_interface;
	pthread_t reading_interface;

	pthread_create(&writing_interface,NULL,handle_output,output_subroutine_arguments);
	pthread_create(&reading_interface,NULL,handle_input,input_subroutine_arguments);
	
	/*
	 *	Setup socket
	 */
	int server_socket = socket(AF_INET,SOCK_STREAM,0);
	
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htonl(2096);
	socklen_t address_length = sizeof(address);

	bind(server_socket,(struct sockaddr*)&address,address_length);
	listen(server_socket,128);
	
	/*
	 *	Run event pool
	 */
	main_poll->poll_buffer[0].fd = server_socket;
	main_poll->poll_buffer[0].events = POLLIN;
	

	for (;;)
	{
		struct pollfd* new_connections;
		int	       new_connections_size = 0;


		int ready = poll(main_poll->poll_buffer,main_poll->size,-1);

		if (ready == -1)
		{
			exit(EXIT_FAILURE);
		}

		for (int i=0; i<main_poll->size; i++)
		{
			
			if (main_poll->poll_buffer[i].revents != 0)
			{
				if (i == 0)
				{
					int new_confd = accept(server_socket,(struct sockaddr*)&address,&address_length);
					new_connections_size++;
					new_connections = realloc(new_connections,sizeof(struct pollfd)*new_connections_size);
					new_connections[new_connections_size-1].fd = new_confd;
					new_connections[new_connections_size-1].events = POLLIN;
					continue;
				}
				
				char* packet_buf = malloc(SIZE);
				ssize_t bytes_read = recv(main_poll->poll_buffer[i].fd,packet_buf,SIZE,MSG_DONTWAIT);

				if (bytes_read == -1)
				{
					free(packet_buf);
					continue;
				}
				packet_buf = realloc(packet_buf,bytes_read);	

				packet_log->size++;
				packet_log->packets = realloc(packet_log->packets,packet_log->size*sizeof(Packet));
				packet_log->packets[packet_log->size-1].connection_fd = main_poll->poll_buffer[i].fd;
				packet_log->packets[packet_log->size-1].bytes = packet_buf;
				packet_log->packets[packet_log->size-1].size = bytes_read;
			}

		}
		
		if (new_connections_size == 0) 
			continue;

		if (main_poll->size > MAX_LOOP)
		{
			for (int i=0; i<connection_polls.size; i++)
			{
				//TODO	
			}
			continue;	
		}


		main_poll->poll_buffer = realloc(main_poll->poll_buffer, (main_poll->size+new_connections_size)*sizeof(struct pollfd));
		for (int i=0; i<new_connections_size; i++)
		{
			fcntl(new_connections[i].fd, F_SETFL, fcntl(new_connections[i].fd, F_GETFL, 0) | O_NONBLOCK);
		}
		memcpy(&main_poll[main_poll->size-1],new_connections,new_connections_size*sizeof(struct pollfd));
		main_poll->size += new_connections_size;
	}
}
