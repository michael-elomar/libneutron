#ifndef _NODE_H_
#define _NODE_H_

#include <neutron.h>

#define MAX_SERVER_CONNECTIONS 16

struct neutron_conn {
	struct {
		uint8_t *data;
		size_t datalen;
		size_t capacity;
	} readbuf;

	struct neutron_conn *next;
};

struct neutron_node {
	struct neutron_loop *loop;

	enum neutron_node_type type;

	struct {
		int fd;
		struct sockaddr_storage *local_addr;
		socklen_t local_addrlen;
	} socket;

	void *userdata;

	int destroy_loop;

	neutron_socket_fd_cb socket_fd_cb;

	neutron_socket_event_cb socket_event_cb;

	neutron_socket_data_cb socket_data_cb;

	struct neutron_conn *head;
};

void listen_cb(int server_fd, uint32_t revents, void *userdata);

#endif
