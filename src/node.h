#ifndef _NODE_H_
#define _NODE_H_

#include <neutron.h>
struct neutron_node {
	struct neutron_loop *loop;

	struct {
		int fd;
		struct sockaddr_storage local_addr;
		socklen_t local_addrlen;
	} socket;

	neutron_socket_fd_cb socket_fd_cb;
};

#endif
