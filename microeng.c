#include <stdio.h>
#include <sys/epoll.h>
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

#include "vec.h"
#include "packet_handler.h"
#include "connection_handler.h"
#include "ipc.h"
#include "context.h"

#define PACKET_MAX 5*1024*1024

int
create_socket()
{
	int server_socket = socket(AF_INET,SOCK_STREAM,0);
	
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(2096);

	bind(server_socket,(struct sockaddr*)&address,sizeof(address));
	fcntl(server_socket, F_SETFL, fcntl(server_socket, F_GETFL, 0) | O_NONBLOCK);
	listen(server_socket,128);
	return server_socket;
}



void
main(void)
{
	//vector_module_test();
	//void* write = get_shared_mem("/microengwrites", PACKET_MAX);
	//void* read = get_shared_mem("/microengreads", PACKET_MAX);
	
	int server = create_socket();
	
	vector* contexts = vector_init(sizeof(context_runtime)); 

	puts("MicroENG");
	

	context* curr_ctx = NULL;
	for (;;) // Interface
	{
		/*
		vector* packet_log = vector_init(sizeof(packet));
		connection_handler* client_handler = malloc(sizeof(connection_handler));
		client_handler->server_fd = server;
		client_handler->packet_log = packet_log;
	
		pthread_mutex_t* packetlog_mutex = malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(packetlog_mutex, NULL);

		client_handler->mutex = packetlog_mutex;

		pthread_t handler_thread;
		pthreaid_create(&handler_thread, NULL, connection_loop, client_handler);
		*/
		
		// Read from STDIN
		size_t size = 16;
		char* curr_line = malloc(size);
		ssize_t bytes_read = getline(&curr_line, &size, stdin);

		if ( bytes_read < 1 )
		{
			free(curr_line);
			continue;
		}
		
		char* start = malloc(8);
		strncpy(start,curr_line,8);
		
		if ( strcmp(start, "get_spin") == 0 )
		{
			puts(curr_ctx->ipc.read.spinlock ? "yes" : "no");
			free(curr_line);
			continue;
		}

		if ( strcmp(start, "get_name") == 0 )
		{
			puts(curr_ctx->name);	
			free(curr_line);
			continue;
		}
		
		if ( strcmp(start, "get_pfix") == 0 )
		{
			puts((char*)&curr_ctx->client_handler->prefix);
			free(curr_line);
			continue;
		}

		if ( strcmp(start, "strt_ctx") == 0 )
		{
			curr_ctx = context_init(server);
			free(curr_line);
			continue;
		}
		
		if ( strcmp(start, "chng_ctx") == 0 )
		{
			curr_ctx = ((context_runtime*)vector_get(contexts,atoi(curr_line+9)))->ctx;
			free(curr_line);
			continue;
		}
		
		if ( strcmp(start, "pfix_ctx") == 0 )
		{
			context_prefix(curr_ctx,atoi(curr_line+9));
			free(curr_line);
			continue;
		}

		if ( strcmp(start,"name_ctx") == 0 )
		{
			*(curr_line+strlen(curr_line)-1) = '\0'; //Take out the \n at the end
			context_name(curr_ctx,curr_line+9);
			continue;
		}

		if ( strcmp(start, "lnch_ctx") == 0 )
		{
			context_runtime* curr_runtime = launch_context(curr_ctx);
			
			if ( curr_runtime == NULL )
			{
				free(curr_line);
				continue;
			}

			vector_push(contexts, curr_runtime);
			free(curr_line);
			continue;
		}
		
		if ( strcmp(start, "spin_ctx") == 0 )
		{
			context_spin(curr_ctx);	
			free(curr_line);
			continue;
		}
		
		if ( strcmp(start, "stop_ctx") == 0 )
		{
			context_stop(curr_ctx);
		}
		
		free(curr_line);
	}
}
