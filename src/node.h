#ifndef _NODE_H_
#define _NODE_H_

#include <neutron.h>
#include <arpa/inet.h>
#include <sys/socket.h>

struct neutron_node {
	struct neutron_loop *loop;

	struct {
		int fd;
		struct sockaddr_storage local_addr;
		socklen_t local_addrlen;
	} socket;
};

#endif
