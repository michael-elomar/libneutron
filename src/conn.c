#include <conn.h>
#include <ctx.h>
#include <priv.h>

struct neutron_conn *neutron_conn_new(int capacity)
{
	struct neutron_conn *conn = NULL;

	conn = (struct neutron_conn *)calloc(1, sizeof(struct neutron_conn));
	if (!conn) {
		LOG_ERRNO("Failed to allocate memory for conn");
		return NULL;
	}

	conn->readbuf.data = (uint8_t *)malloc(capacity);
	if (!conn->readbuf.data) {
		LOG_ERRNO("Failed to allocate memory for readbuf in conn");
		free(conn);
		conn = NULL;
		return NULL;
	}

	conn->readbuf.capacity = capacity;
	conn->remove = 0;
	conn->fd = -1;

	conn->local = (struct sockaddr_storage *)calloc(
		1, sizeof(struct sockaddr_storage));

	if (!conn->local) {
		LOG_ERRNO(
			"Failed to allocate memory for local address in conn");
		free(conn);
		conn = NULL;
		return NULL;
	}

	conn->peer = (struct sockaddr_storage *)calloc(
		1, sizeof(struct sockaddr_storage));

	if (!conn->peer) {
		LOG_ERRNO(
			"Failed to allocate memory for remote peer address in conn");
		free(conn->local);
		conn->local = NULL;
		free(conn);
		conn = NULL;
		return NULL;
	}

	return conn;
}

static void conn_process_write(struct neutron_ctx *ctx,
			       struct neutron_conn *conn)
{
	/* TODO */
}

static void conn_process_read_stream(struct neutron_conn *conn)
{
	do {
		conn->readbuf.datalen = read(
			conn->fd, conn->readbuf.data, conn->readbuf.capacity);
	} while (conn->readbuf.datalen < 0 && errno != EINTR);

	if (conn->readbuf.datalen > 0) {
		int ret = neutron_ctx_notify_event(
			conn->ctx, NEUTRON_EVENT_DATA, conn);
		if (ret) {
			LOG_ERRNO("Failed to notify msg event");
			return;
		}

		if (conn->ctx->data_cb) {
			(*conn->ctx->data_cb)(conn->ctx,
					      conn,
					      conn->readbuf.data,
					      conn->readbuf.datalen,
					      conn->ctx->userdata);
		}
	} else {
		conn->remove = 1;
	}
}

static void conn_process_read_dgram(struct neutron_conn *conn)
{
	do {
		conn->readbuf.datalen = recvfrom(conn->fd,
						 conn->readbuf.data,
						 conn->readbuf.capacity,
						 0,
						 (struct sockaddr *)conn->peer,
						 &conn->peer_addrlen);
	} while (conn->readbuf.datalen < 0 && errno != EINTR);

	if (conn->readbuf.datalen > 0) {
		int ret = neutron_ctx_notify_event(
			conn->ctx, NEUTRON_EVENT_DATA, conn);
		if (ret) {
			LOG_ERRNO("Failed to notify udp msg event");
			return;
		}
	}

	if (conn->ctx->data_cb) {
		(*conn->ctx->data_cb)(conn->ctx,
				      conn,
				      conn->readbuf.data,
				      conn->readbuf.datalen,
				      conn->ctx->userdata);
	}
}

static void conn_process_read(struct neutron_ctx *ctx,
			      struct neutron_conn *conn)
{
	if (ctx->type == NEUTRON_DGRAM)
		conn_process_read_dgram(conn);
	else
		conn_process_read_stream(conn);
}

void conn_cb(int fd, uint32_t revents, void *userdata)
{
	struct neutron_ctx *ctx = (struct neutron_ctx *)userdata;
	struct neutron_conn *conn = neutron_ctx_find_connection(ctx, fd);

	if (!conn->remove && (revents & NEUTRON_FD_EVENT_IN))
		conn_process_read(ctx, conn);
	else if (!conn->remove && (revents & NEUTRON_FD_EVENT_OUT))
		conn_process_write(ctx, conn);
	else if (conn->remove || NEUTRON_FD_EVENT_ERROR)
		neutron_ctx_remove_conn(ctx, conn);
}
