#ifndef _CONN_H_
#define _CONN_H_

#include <neutron.h>

struct neutron_conn {
	struct {
		uint8_t *data;
		size_t datalen;
		size_t capacity;
	} readbuf;

	int type;

	int fd;

	uint8_t remove;

	struct sockaddr_storage *local, *peer;
	socklen_t local_addlren, peer_addrlen;

	struct neutron_conn *next;

	struct neutron_ctx *ctx;
};

struct neutron_conn *neutron_conn_new(int capacity);

void neutron_conn_destroy(struct neutron_conn *conn);

int neutron_ctx_remove_conn(struct neutron_ctx *ctx, struct neutron_conn *conn);

void conn_cb(int fd, uint32_t revents, void *userdata);

#endif
