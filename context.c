#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>

#include "hash_table.h"
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
	close(fd);
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
context_init(int server_fd,hash_table* prefix)
{
	context* output = malloc(sizeof(context));
	output->server_fd = server_fd;
	output->mutex = malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(output->mutex, NULL);
	output->packet_log = vector_init(sizeof(packet));
	output->client_handler = connection_handler_init(server_fd, output->packet_log, output->mutex,prefix);
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
context_read_spin(context* ctx)
{
	ctx->ipc.read.spinlock = true;
	pthread_cond_signal(&ctx->ipc.read.spin_cond);
}

void
context_write_spin(context* ctx)
{
	ctx->ipc.write.spinlock = true;
	pthread_cond_signal(&ctx->ipc.write.spin_cond);
}
void
context_read_stop(context* ctx)
{
	ctx->ipc.read.spinlock = false;
}
void
context_write_stop(context* ctx)
{
	ctx->ipc.write.spinlock = false;
}
void
context_stop(context* ctx)
{
	ctx->ipc.read.spinlock = false;
	ctx->ipc.write.spinlock = false;
}

void
context_prefix(context* ctx, int prefix)
{
	pthread_mutex_lock(ctx->mutex);
	char prefix_str[sizeof(int)+1];
	strncpy(prefix_str,(char*)&ctx->client_handler->prefix,sizeof(int));
	prefix_str[sizeof(int)] = '\0';
	hash_table_unset(ctx->client_handler->packet_log_table,prefix_str);
	strncpy(prefix_str,(char*)&prefix,sizeof(int));
	packet_log new_log = {.packets=ctx->packet_log,.mutex=ctx->mutex,.new_packet=ctx->client_handler->new_packet};
	hash_table_set(ctx->client_handler->packet_log_table,prefix_str,&new_log,sizeof(packet_log));
	ctx->client_handler->prefix = prefix;
	pthread_mutex_unlock(ctx->mutex);
}
