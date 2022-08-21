typedef struct connection_handler {
	int server_fd;
	pthread_mutex_t* mutex;
	pthread_cond_t* new_packet;
	vector* packet_log;
	int prefix;
} connection_handler;

connection_handler* connection_handler_init(int, vector*, pthread_mutex_t*);
void* connection_loop(void* args);
