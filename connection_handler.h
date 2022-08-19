typedef struct connection_handler {
	int server_fd;
	pthread_mutex_t* mutex;
	vector* packet_log;
} connection_handler;

connection_handler* connection_handler_init(int, vector*, pthread_mutex_t*);
void* connection_loop(void* args);
