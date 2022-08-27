typedef struct packet_log {
	vector* packets;
	pthread_mutex_t* mutex;
	pthread_cond_t* new_packet;
} packet_log;

typedef struct packet {
	int fd;
	char* bytes;
	size_t size;
} packet;

typedef struct packet_handler {
	pthread_t thread;
	pthread_mutex_t* mutex;
	vector* packet_log;
	int num_conns;
	int epoll_fd;
	hash_table* packet_log_table;
	pthread_cond_t* new_packet;
} handler;

void print_packet(packet);
handler* handler_init();
void* handle_packets(void* args);
