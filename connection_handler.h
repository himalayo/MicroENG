typedef struct connection_handler {
	int server_fd;
	pthread_mutex_t* mutex;
	pthread_cond_t* new_packet;
	vector* packet_log;
	int prefix;
	hash_table* packet_log_table;
} connection_handler;

connection_handler* connection_handler_init(int, vector*, pthread_mutex_t*,hash_table*);
void* connection_loop(void* args);
