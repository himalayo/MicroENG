#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>

#include "vec.h"
#include "packet_handler.h"
#include "connection_handler.h"
#include "ipc.h"
#include "context.h"

void*
get_shared_mem(const char* filename,size_t size)
{
	int fd = shm_open(filename,O_RDWR|O_CREAT,S_IRWXU|S_IRWXG|S_IRWXO);
	ftruncate(fd,size);
	void* output = mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
	memset(output,0,size);
	return output;
}

context_runtime*
launch_context(context* ctx)
{

	if ( ctx->launched )
	{
		return NULL;
	}

	context_runtime* output = malloc(sizeof(context_runtime));
	pthread_create(&output->client_handler_thread, NULL, connection_loop,ctx->client_handler);
	pthread_create(&output->ipc_read_thread, NULL, ipc_read,ctx);
	pthread_create(&output->ipc_write_thread, NULL, ipc_write,ctx);
	output->ctx = ctx;
	
	ctx->launched = true;

	return output;
}

context*
context_init(int server_fd)
{
	context* output = malloc(sizeof(context));
	output->server_fd = server_fd;
	output->mutex = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(output->mutex, NULL);
	output->packet_log = vector_init(sizeof(packet));
	output->client_handler = connection_handler_init(server_fd, output->packet_log, output->mutex);
	output->name = NULL;
	return output;
}

void
context_name(context* ctx, char* name)
{
	if ( strchr(name, ' ') != NULL ) 
		return;
	if ( strlen(name)+strlen("microengwrites") >= 255 )
		return;
	if ( ctx->name != NULL )
	{
		char old_writes[255];
		char old_reads[255];
			
		strcpy(old_writes,ctx->name);
		strcpy(old_reads,ctx->name);
		strcat(old_writes,"microengwrites");
		strcat(old_reads,"microengreads");
		context_stop(ctx);
		shm_unlink(old_writes);
		shm_unlink(old_reads);
		
		free(ctx->name);
	}

	ctx->name = name;

	char writes[255];
	char reads[255];
	strcpy(writes,name);
	strcpy(reads, name);
	strcat(writes, "microengwrites");
	strcat(reads,"microengreads");
	
	ctx->ipc = ipc_data_init(get_shared_mem(writes, 5*1024*1024), get_shared_mem(reads,5*1024*102));

	#ifdef DEBUG
	strcat(writes,"\n");
	write(STDOUT_FILENO,writes,strlen(writes));
	#endif
}

void
context_spin(context* ctx)
{
	ctx->ipc.read.spinlock = true;
	ctx->ipc.write.spinlock = true;

	pthread_cond_signal(&ctx->ipc.read.spin_cond);
	pthread_cond_signal(&ctx->ipc.write.spin_cond);
}

void
context_stop(context* ctx)
{
	ctx->ipc.read.spinlock = false;
	ctx->ipc.write.spinlock = false;
}
