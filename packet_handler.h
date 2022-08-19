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
} handler;

void print_packet(packet);
handler* handler_init();
void* handle_packets(void* args);
