#ifndef _NODE_H_
#define _NODE_H_

#include <neutron.h>

#define MAX_SERVER_CONNECTIONS 16

struct neutron_node {
	struct neutron_loop *loop;

	struct {
		int fd;
		struct sockaddr_storage *local_addr;
		socklen_t local_addrlen;
	} socket;

	int destroy_loop;

	neutron_socket_fd_cb socket_fd_cb;
};

#endif
