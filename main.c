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

#ifdef USE_POLL
#include <poll.h>
#endif
#ifndef USE_POLL
#include <sys/epoll.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#define EVENT_BUFFER_LEN 32
#define MAX_LOOP 50

#define MAX_PACKET_HIST 10 
//Packet buffer, and shared memory size
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
	Packet** packets;
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

#ifdef USE_POLL
typedef struct connection_poll
{
	struct pollfd* poll_buffer;
	size_t size;
} ConnectionPoll;
#endif

#ifndef USE_POLL
typedef struct connection_epoll
{
	struct epoll_event event_buffer[EVENT_BUFFER_LEN];
	int fd;
} ConnectionEPoll;
#endif

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
				input->packet_list->packets[write_header->argument]->bytes,
				input->packet_list->packets[write_header->argument]->size
			);
			write_header->size = input->packet_list->packets[write_header->argument]->size;
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

#ifdef USE_POLL
void*
secondary_event_poll(void* args)
{
	ConnectionPoll* conn_poll = (ConnectionPoll*)args;
	for(;;)
	{
		int ready = poll(conn_poll->poll_buffer,conn_poll->size,-1);

		for (int i=0; i<conn_poll->size; i++)
		{
			if (conn_poll->poll_buffer[i].revents == 0)
				continue;
			
				
		}
			
	}
	return NULL;
}
#endif

/*
 *	Boilerplate
 */
struct epoll_event
create_events(int fd, unsigned int events) {
	struct epoll_event output;
	output.events = events;
	output.data.fd = fd;
	return output;
}

/*
 *	Takes a heap-allocated buffer and turns it into a packet.
 */
Packet*
into_packet(int in_fd, char* buff, size_t buff_len)
{
	Packet* output = malloc(sizeof(Packet));
	output->connection_fd = in_fd;
	output->size = buff_len;
	output->bytes = buff;
	return output;
}

/*
 *	Copies a stack-allocated buffer into the heap and returns it as a new packet.
 */
Packet*
new_packet(int in_fd, char* buff, size_t buff_len)
{
	Packet* output = malloc(sizeof(Packet));
	output->connection_fd = in_fd;
	output->size = buff_len;
	output->bytes = malloc(buff_len);
	memcpy(output->bytes, buff, buff_len);
	return output;
}

/*
 *	Appends a packet into packet history
 */
void
append_packet(PacketHistory* hist, Packet* packet)
{
	hist->size++;
	hist->packets = realloc(hist->packets, hist->size*sizeof(Packet));
	hist->packets[hist->size-1] = packet;
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
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(2096);

	bind(server_socket,(struct sockaddr*)&address,sizeof(address));
	
	#ifdef USE_POLL
	listen(server_socket,128);
	/*
	 *	Run event pool
	 */
	ConnectionPoll* main_poll = malloc(sizeof(ConnectionPoll));
	main_poll->poll_buffer = malloc(sizeof(struct pollfd));

	main_poll->poll_buffer[0].fd = server_socket;
	main_poll->poll_buffer[0].events = POLLIN;
	main_poll->size = 1;
	
	struct poll_list {
		pthread_t*      thread_list;
		size_t          thread_list_size;
		ConnectionPoll* polls;
		size_t          size;
	} connection_polls;

	
	for (;;)
	{
		/*
		struct pollfd* new_connections = malloc(sizeof(struct pollfd));
		int	       new_connections_size = 0;
		*/

		int ready = poll(main_poll->poll_buffer,main_poll->size,-1);

		if (ready == -1)
		{
			exit(EXIT_FAILURE);
		}
		if (main_poll->poll_buffer[0].revents & POLLIN) //Accept incoming connections
		{
			struct sockaddr_in client_addr;
			int client_addr_len = sizeof(client_addr);
			int new_confd = accept(server_socket,(struct sockaddr*)&client_addr,&client_addr_len);
			
			/*
			new_connections_size++;
			new_connections = realloc(new_connections,sizeof(struct pollfd)*new_connections_size);
			new_connections[new_connections_size-1].fd = new_confd;
			new_connections[new_connections_size-1].events = POLLIN;
			*/
			main_poll->size++;
			main_poll->poll_buffer = realloc(main_poll->poll_buffer,main_poll->size*sizeof(struct pollfd));
			main_poll->poll_buffer[main_poll->size-1].fd = new_confd;
			main_poll->poll_buffer[main_poll->size-1].events = POLLIN;
		}
		

		for (int i=1; i<main_poll->size; i++)
		{

			if (main_poll->poll_buffer[i].revents & POLLIN)
			{
				//Receive packets	
				char *packet_buf = malloc(SIZE); 
				ssize_t bytes_read = recv(main_poll->poll_buffer[i].fd,packet_buf,SIZE,0);

				if (bytes_read == -1 || bytes_read == 0)
				{
					free(packet_buf);
					close(main_poll->poll_buffer[i].fd);
					main_poll->size--;
					memmove(&main_poll->poll_buffer[i],&main_poll->poll_buffer[i+1],main_poll->size-i);
					continue;
				}
				
				packet_buf = realloc(packet_buf, bytes_read);
				append_packet(packet_log, into_packet(main_poll->pull_buffer[i].fd,packet_buf, bytes_read));

				write(STDOUT_FILENO, packet_log->packets[packet_log->size-1].bytes, packet_log->packets[packet_log->size-1].size);
			}

		}
		
		//Check whether we have new connections to append to the event loop
		/*
		if (new_connections_size == 0) 
			continue;
		*/
		//Append incoming connections into other polls in case ours is full	
		/*
		if (main_poll->size > MAX_LOOP)
		{
			bool found_poll = false;
			for (int i=0; i<connection_polls.size; i++)
			{
				if (connection_polls.polls[i].size < MAX_LOOP)
				{
					connection_polls.polls[i].poll_buffer = realloc(connection_polls.polls[i].poll_buffer,sizeof(struct pollfd)*(connection_polls.polls[i].size+new_connections_size));
					memcpy(connection_polls.polls[i].poll_buffer+connection_polls.polls[i].size,new_connections,new_connections_size*sizeof(struct pollfd));
					connection_polls.polls[i].size += new_connections_size;
					
					found_poll = true;
					
					break;
				}
			}
			
			//Create new poll in seperate thread i case every other poll is already filled
			if (!found_poll)
			{
				connection_polls.thread_list_size++;
				connection_polls.thread_list = realloc(connection_polls.thread_list,sizeof(pthread_t)*connection_polls.thread_list_size);
				pthread_create(&connection_polls.thread_list[connection_polls.thread_list_size-1],NULL,secondary_event_poll,(void*)&connection_polls.polls[connection_polls.size-1]);
			}
			continue;	
		}
		*/
		//Append the new connection to our poll in case it's not full
		/*
		main_poll->poll_buffer = realloc(main_poll->poll_buffer, (main_poll->size+new_connections_size)*sizeof(struct pollfd));
		for (int i=0; i<new_connections_size; i++)
		{
			fcntl(new_connections[i].fd, F_SETFL, fcntl(new_connections[i].fd, F_GETFL, 0) | O_NONBLOCK);
		}
		memcpy(&main_poll->poll_buffer[main_poll->size],new_connections,new_connections_size*sizeof(struct pollfd));
		main_poll->size += new_connections_size;
		*/
	}
	#endif

	#ifndef USE_POLL
	ConnectionEPoll main_epoll;

	fcntl(server_socket, F_SETFL, fcntl(server_socket, F_GETFL, 0) | O_NONBLOCK);
	listen(server_socket,128);

	main_epoll.fd = epoll_create(1);
	
	struct epoll_event server_events = create_events(server_socket, EPOLLIN | EPOLLOUT | EPOLLET);
	epoll_ctl(main_epoll.fd, EPOLL_CTL_ADD, server_socket, &server_events);
	
	for (;;)
	{
		int num_events = epoll_wait(main_epoll.fd, main_epoll.event_buffer, EVENT_BUFFER_LEN, -1);
		
		for (int i=0; i < num_events; i++)
		{
			if ( main_epoll.event_buffer[i].data.fd == server_socket )
			{
				struct sockaddr_in new_connection_address;
				int new_connection_len = sizeof(new_connection_address);
				int new_connection_fd = accept(server_socket, (struct sockaddr*)&new_connection_address, &new_connection_len);	
				fcntl(new_connection_fd, F_SETFL, fcntl(new_connection_fd, F_GETFL, 0) | O_NONBLOCK);
				
				struct epoll_event client_events = create_events(new_connection_fd, EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLET);
				epoll_ctl(main_epoll.fd, EPOLL_CTL_ADD, new_connection_fd, &client_events);
				continue;
			}

			if ( main_epoll.event_buffer[i].events & EPOLLIN )
			{
				char *packet_buff = malloc(SIZE);
				ssize_t bytes_read = recv(main_epoll.event_buffer[i].data.fd, packet_buff, SIZE, 0);

				if (bytes_read == -1)
				{
					free(packet_buff);
					continue;
				}
				packet_buff = realloc(packet_buff, bytes_read);
				append_packet(packet_log, into_packet(main_epoll.event_buffer[i].data.fd, packet_buff,bytes_read));
				continue;
			}

			if ( main_epoll.event_buffer[i].events & (EPOLLRDHUP | EPOLLHUP) )
			{
				epoll_ctl(main_epoll.fd, EPOLL_CTL_DEL, main_epoll.event_buffer[i].data.fd, NULL);
				close(main_epoll.event_buffer[i].data.fd);	
			}
		}
	}
	#endif
}
