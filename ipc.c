#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>

#include "vec.h"
#include "packet_handler.h"
#include "connection_handler.h"
#include "ipc.h"
#include "context.h"

typedef struct ipc_header {
	char opcode;
	int args;
	size_t len;
} ipc_header;

#define HEADER_SEND 1

#define HEADER_POP 1
#define HEADER_DONE 1<<2

ipc_routine
ipc_routine_init(void* buffer)
{
	ipc_routine output = {.buffer=buffer,.spinlock=false,};
	pthread_mutex_init(&output.mutex, NULL);
	pthread_cond_init(&output.spin_cond, NULL);
	return output;
}

ipc_data
ipc_data_init(void* write, void* read)
{
	return (ipc_data){.write=ipc_routine_init(write), .read=ipc_routine_init(read)};
}

void*
ipc_read(void* args)
{
	context* ctx = (context*)args;

	#ifdef DEBUG
	char test_message[60] = "\n";
	size_t size = strlen(test_message);
	#endif

	for (;;)
	{
		pthread_mutex_lock(&ctx->ipc.read.mutex);
		while (ctx->ipc.read.spinlock == false)
		{
			pthread_cond_wait(&ctx->ipc.read.spin_cond, &ctx->ipc.read.mutex);
		}
		pthread_mutex_unlock(&ctx->ipc.read.mutex);
	
		#ifdef DEBUG
		memset(test_message,'\0',size);
		strcpy(test_message,"started spinning\n");
		size = strlen(test_message);
		write(STDOUT_FILENO,test_message,size);
		#endif
	
		ipc_header* header = ctx->ipc.read.buffer;
		while(ctx->ipc.read.spinlock == true)
		{
			switch(header->opcode)
			{
				case HEADER_POP:
								
					pthread_mutex_lock(ctx->mutex);
					packet* last_packet = vector_pop(ctx->packet_log);
					pthread_mutex_unlock(ctx->mutex);
					
					memcpy((char*)(ctx->ipc.read.buffer+sizeof(ipc_header)), last_packet->bytes, last_packet->size);
					header->args = last_packet->fd;
					header->len = last_packet->size;
					header->opcode = HEADER_DONE;
					break;
				default:
					break;
			}
		}

		#ifdef DEBUG
		memset(test_message,'\0',size);
		strcpy(test_message,"stopped spinning\n");
		size = strlen(test_message);
		write(STDOUT_FILENO,test_message,size);	
		#endif
	}

	return NULL;
}

void*
ipc_write(void* args)
{
	context* ctx = (context*)args;

	for (;;)
	{
		pthread_mutex_lock(&ctx->ipc.write.mutex);
		while (ctx->ipc.write.spinlock == false)
		{
			pthread_cond_wait(&ctx->ipc.write.spin_cond, &ctx->ipc.write.mutex);
		}
		pthread_mutex_unlock(&ctx->ipc.write.mutex);
		
		ipc_header* header = ctx->ipc.write.buffer;
		while(ctx->ipc.write.spinlock == true)
		{
			switch(header->opcode)
			{
				case HEADER_SEND:

					send(
						header->args,
						ctx->ipc.write.buffer+sizeof(ipc_header),
						header->len,
						0	
					);

					header->opcode = HEADER_DONE;	
					break;
				default:
					break;
			}
		}

	}

	return NULL;
}
