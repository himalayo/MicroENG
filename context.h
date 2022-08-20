typedef struct ctx {
	bool launched;
	char* name;
	int server_fd;
	vector* packet_log;
	pthread_mutex_t* mutex;
	connection_handler* client_handler;
	ipc_data ipc;
} context;

typedef struct ctx_run 
{
	pthread_t client_handler_thread;
	pthread_t ipc_read_thread;
	pthread_t ipc_write_thread;
	context* ctx;
} context_runtime;

context_runtime* launch_context(context*);
context* context_init(int);
void context_spin(context*);
void context_stop(context*);
void context_name(context*,char*);
void context_prefix(context*,int);
