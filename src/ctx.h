#ifndef _CTX_H_
#define _CTX_H_

#include <neutron.h>

#define MAX_SERVER_CONNECTIONS 16

struct neutron_ctx {
	struct neutron_loop *loop;

	enum neutron_ctx_type type;

	struct {
		int fd;
		struct sockaddr_storage *addr;
		socklen_t addrlen;
		int type;
	} socket;

	void *userdata;

	int destroy_loop;

	neutron_ctx_fd_cb fd_cb;

	neutron_ctx_event_cb event_cb;

	neutron_ctx_data_cb data_cb;

	struct neutron_conn *head;
};

struct neutron_conn *neutron_ctx_find_connection(struct neutron_ctx *ctx,
						 int fd);

int neutron_ctx_notify_event(struct neutron_ctx *ctx,
			     enum neutron_event,
			     struct neutron_conn *conn);

void listen_cb(int server_fd, uint32_t revents, void *userdata);

void connect_cb(int conn_fd, uint32_t revents, void *userdata);

#endif
