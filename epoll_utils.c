#include <sys/epoll.h>

#include "epoll_utils.h"

void
epoll_add(int epoll_fd, int fd,int events)
{
	struct epoll_event event = {.data.fd=fd,.events=events};
	epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&event);
}

int
create_epoll(int fd, int events)
{
	if (events == -1)
	{
		events = EPOLLIN | EPOLLET;
	}

	int output = epoll_create(1);
	epoll_add(output, fd, events);
	return output;
}
