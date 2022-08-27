#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>

#include "hash_table.h"
#include "vec.h"
#include "epoll_utils.h"
#include "packet_handler.h"
#include "connection_handler.h"

//	This is the answer to the ultimate question of Life, the universe, and everything.
//	I think that implies it will give us better perfomance.
#define MAX_CONNS_AT_ONCE 42


connection_handler*
connection_handler_init(int server_fd, vector* packet_log, pthread_mutex_t* mutex,hash_table* packet_log_table)
{
	connection_handler* output = malloc(sizeof(connection_handler));
	output->server_fd = server_fd;
	output->packet_log_table = packet_log_table;
	output->packet_log = packet_log;
	output->mutex = mutex;
	output->new_packet = malloc(sizeof(pthread_cond_t));
	pthread_cond_init(output->new_packet, NULL);
	return output;
}

/*
 *	Assigns to a connection to an available packet handler
 */
void
assign(vector* handlers, int conn_fd)
{
	//char debug[70];
	handler* top = (handler*)vector_last(handlers);	
	epoll_add(top->epoll_fd, conn_fd, EPOLLET|EPOLLIN|EPOLLHUP|EPOLLRDHUP);		
}

/*
 *	Spawns a new packet handler and pushes it into a packet handler vector
 */
void
spawn_handler(vector* handler_vector,vector* packet_log, pthread_mutex_t* mutex, hash_table* prefix, pthread_cond_t* new_packet)
{
	handler* handler = handler_init();
	handler->packet_log = packet_log;
	handler->mutex = mutex;
	handler->packet_log_table = prefix;
	handler->new_packet = new_packet;
	pthread_create(&handler->thread, NULL, handle_packets, handler);
	vector_push(handler_vector, handler);	
}

/*
 *	Handle connections, by accepting, asigning them into the packet handlers, and spawning new packet handlers when necessary.
 */
void*
connection_loop(void* args)
{
	connection_handler* this = (connection_handler*)args;

	int epoll = create_epoll(this->server_fd, -1);
	vector* active_handlers = vector_init(sizeof(handler));
	vector* available_handlers = vector_init(sizeof(handler));
	
	spawn_handler(active_handlers, this->packet_log, this->mutex,this->packet_log_table, this->new_packet);
	vector_push(available_handlers, vector_last(active_handlers));

	for (;;)
	{
		struct epoll_event events[MAX_CONNS_AT_ONCE];
		int new_connections = epoll_wait(epoll, events, MAX_CONNS_AT_ONCE, -1);
		
		//	Check for available connection handlers
		vector_clear(available_handlers);
		for (int i=0; i<active_handlers->length; i++)
		{
			handler* curr_handler = (handler*)vector_get(active_handlers, i);

			if ( curr_handler->num_conns < MAX_CONNS_AT_ONCE )
			{
				vector_push(available_handlers, curr_handler);
			}
		}

		if ( available_handlers->length == 0 )
		{
			spawn_handler(active_handlers, this->packet_log, this->mutex, this->packet_log_table, this->new_packet);
			vector_push(available_handlers, vector_last(active_handlers));
		}
		
		if ( new_connections == -1 )
		{
			//	Something went wrong :(
			break;
		}
		
		if ( new_connections > 0 )
		{
			struct sockaddr_in new_connection_address;
			int new_connection_len = sizeof(new_connection_address);
			int new_connection_fd = accept(this->server_fd, (struct sockaddr*)&new_connection_address, &new_connection_len);	
			fcntl(new_connection_fd, F_SETFL, fcntl(new_connection_fd, F_GETFL, 0) | O_NONBLOCK);
			assign(available_handlers, new_connection_fd);
		}

	
	}
}
