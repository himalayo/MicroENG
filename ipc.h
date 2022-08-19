typedef struct ipc_routine 
{
	void* buffer;
	bool spinlock;
	pthread_mutex_t mutex;
	pthread_cond_t spin_cond;
} ipc_routine;

typedef struct ipc_data {
	ipc_routine read;
	ipc_routine write;
} ipc_data;

ipc_data ipc_data_init(void*, void*);
void* ipc_read(void*);
void* ipc_write(void*);
