#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <pthread.h>

#include "hash_table.h"
#include "vec.h"
#include "packet_handler.h"

#define MAX_PACKET 5*1024*1024

/*
 *	Allocates a new packet handler
 */
handler*
handler_init()
{
	handler* output = malloc(sizeof(handler));
	output->epoll_fd = epoll_create(1);
	output->num_conns = 0;
	return output;
}

void
print_packet(packet input)
{
	write(STDOUT_FILENO, input.bytes, input.size);
}

void*
handle_packets(void* args)
{
	handler* this = (handler*)args;
	for(;;)
	{	
		struct epoll_event events[42];
		int num_events = epoll_wait(this->epoll_fd,events,42,-1);
		for (int i=0; i<num_events; i++)
		{
			if ( events[i].events & EPOLLIN )
			{
				packet new_packet = {.fd = events[i].data.fd, .bytes = malloc(MAX_PACKET)};
				new_packet.size = recv(events[i].data.fd,new_packet.bytes,MAX_PACKET,MSG_DONTWAIT);
				if ( new_packet.size <= sizeof(int) )
				{
					free(new_packet.bytes);
					continue;
				}
				
				char prefix[sizeof(int)+1];
				strncpy(prefix,new_packet.bytes,sizeof(int));
				prefix[sizeof(int)] = '\0';;
				packet_log* log = hash_table_get(this->packet_log_table,prefix);

				if (log == NULL)
				{
					free(new_packet.bytes);
					continue;
				}

				new_packet.bytes = realloc(new_packet.bytes,new_packet.size);
				pthread_mutex_lock(log->mutex);
				vector_push(log->packets, &new_packet);
				pthread_mutex_unlock(log->mutex);
				pthread_cond_signal(log->new_packet);
				continue;
			}
			
			// This can only be either EPOLLHUP or EPOLLRDHUP.
			close(events[i].data.fd);
			epoll_ctl(this->epoll_fd,EPOLL_CTL_DEL,events[i].data.fd,NULL);
			this->num_conns--;
		}
	}
	return NULL;
}
