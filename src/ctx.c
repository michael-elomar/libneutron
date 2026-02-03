#include <priv.h>
#include <ctx.h>
#include <loop.h>
#include <threads.h>

static void neutron_ctx_add_conn(struct neutron_ctx *ctx,
				 struct neutron_conn *conn)
{
	if (!ctx->head) {
		ctx->head = conn;
		return;
	}

	struct neutron_conn *aux = ctx->head;
	while (aux->next)
		aux = aux->next;

	aux->next = conn;
}
static void conn_process_write(struct neutron_ctx *ctx,
			       struct neutron_conn *conn)
{
	/* TODO */
}

static void conn_process_read_stream(struct neutron_ctx *ctx,
				     struct neutron_conn *conn)
{
	do {
		conn->readbuf.datalen = read(
			conn->fd, conn->readbuf.data, conn->readbuf.capacity);
	} while (conn->readbuf.datalen < 0 && errno != EINTR);

	if (conn->readbuf.datalen > 0) {
		int ret =
			neutron_ctx_notify_event(ctx, NEUTRON_EVENT_DATA, conn);
		if (ret) {
			LOG_ERRNO("Failed to notify msg event");
			return;
		}

		if (ctx->data_cb) {
			(*ctx->data_cb)(ctx,
					conn,
					conn->readbuf.data,
					conn->readbuf.datalen,
					ctx->userdata);
		}
	} else {
		conn->remove = 1;
	}
}

static void conn_process_read_dgram(struct neutron_ctx *ctx,
				    struct neutron_conn *conn)
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
		int ret =
			neutron_ctx_notify_event(ctx, NEUTRON_EVENT_DATA, conn);
		if (ret) {
			LOG_ERRNO("Failed to notify udp msg event");
			return;
		}
	}

	if (ctx->data_cb) {
		(*ctx->data_cb)(ctx,
				conn,
				conn->readbuf.data,
				conn->readbuf.datalen,
				ctx->userdata);
	}
}

static void conn_process_read(struct neutron_ctx *ctx,
			      struct neutron_conn *conn)
{
	if (ctx->type == NEUTRON_DGRAM)
		conn_process_read_dgram(ctx, conn);
	else
		conn_process_read_stream(ctx, conn);
}

static void conn_cb(int fd, uint32_t revents, void *userdata)
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

static int server_accept_conn(int server_fd, struct neutron_ctx *ctx)
{
	int ret = 0, conn_fd = -1;

	conn_fd = accept(server_fd,
			 (struct sockaddr *)ctx->socket.addr,
			 &ctx->socket.addrlen);

	if (conn_fd < 0) {
		ret = errno;
		LOG_ERRNO("Failed to accept connection");
		goto cleanup;
	}

	struct neutron_conn *conn = neutron_conn_new(512);
	conn->fd = conn_fd;
	neutron_ctx_add_conn(ctx, conn);

	ret = neutron_loop_add(
		ctx->loop, conn_fd, conn_cb, NEUTRON_FD_EVENT_IN, ctx);

	if (ret) {
		LOG_ERRNO("Failed to add connection fd to event loop");
		goto cleanup;
	}

	ret = neutron_ctx_notify_event(ctx, NEUTRON_EVENT_CONNECTED, conn);
	if (ret) {
		LOG_ERRNO("Failed to notify connection event");
		return ret;
	}

	return 0;

cleanup:
	if (conn_fd > 0)
		close(conn_fd);

	return ret;
}

int neutron_ctx_notify_event(struct neutron_ctx *ctx,
			     enum neutron_event event,
			     struct neutron_conn *conn)
{
	if (!ctx) {
		VLOGE("neutron ctx is null");
		return EINVAL;
	}

	if (!conn) {
		VLOGE("neutron conn is null");
		return EINVAL;
	}

	if (ctx->event_cb) {
		(*ctx->event_cb)(ctx, event, conn, ctx->userdata);
	}

	return 0;
}

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

void neutron_conn_destroy(struct neutron_conn *conn)
{
	if (conn) {
		if (conn->readbuf.data) {
			free(conn->readbuf.data);
			conn->readbuf.data = NULL;
		}

		if (conn->local) {
			free(conn->local);
			conn->local = NULL;
		}

		if (conn->peer) {
			free(conn->peer);
			conn->peer = NULL;
		}

		if (conn->fd > 0) {
			close(conn->fd);
			conn->fd = -1;
		}

		conn->readbuf.capacity = 0;
		conn->readbuf.datalen = 0;

		free(conn);
		conn = NULL;
	}
}

struct neutron_ctx *neutron_ctx_create_with_loop(neutron_ctx_event_cb cb,
						 struct neutron_loop *loop,
						 void *userdata)
{
	struct neutron_ctx *ctx =
		(struct neutron_ctx *)calloc(1, sizeof(struct neutron_ctx));

	if (!ctx) {
		LOG_ERRNO("Failed to allocate memory for new node");
		goto cleanup;
	}

	ctx->event_cb = cb;
	ctx->loop = loop;
	ctx->userdata = userdata;
	ctx->destroy_loop = 0;
	return ctx;

cleanup:
	neutron_ctx_destroy(ctx);
	return NULL;
}

struct neutron_ctx *neutron_ctx_create(neutron_ctx_event_cb cb, void *userdata)
{
	struct neutron_loop *loop = neutron_loop_create();

	if (!loop) {
		LOGE("Failed to create neutron_loop for node");
		goto cleanup;
	}

	struct neutron_ctx *ctx =
		neutron_ctx_create_with_loop(cb, loop, userdata);

	if (!ctx) {
		LOGE("Failed to create neutron context");
		goto cleanup;
	}

	ctx->destroy_loop = 1;

	return ctx;

cleanup:
	neutron_ctx_destroy(ctx);
	neutron_loop_destroy(loop);
	return NULL;
}

int neutron_ctx_parse_address(const char *address,
			      struct sockaddr_storage *addr,
			      socklen_t *addrlen)
{
	if (strlen(address) > 64)
		return EINVAL;

	if (!addr)
		return EINVAL;

	int ret = 0;

	char *addr_cpy = strdup(address);
	char *protocol = strtok(addr_cpy, ":");
	if (!protocol) {
		LOGE("Address format incorrect: failed to parse protocol");
		ret = EINVAL;
		goto cleanup;
	}

	char *addr_body = strtok(NULL, ":");
	if (!addr_body) {
		LOGE("Address format incorrect: failed to parse address");
		ret = EINVAL;
		goto cleanup;
	}

	if (strcmp(protocol, "unix") == 0) {
		struct sockaddr_un *sock_un;
		sock_un = (struct sockaddr_un *)addr;

		if (addr_body[0] == '/')
			sprintf(sock_un->sun_path, "%s", addr_body);
		else
			sprintf(sock_un->sun_path, "/tmp/%s", addr_body);

		sock_un->sun_family = AF_UNIX;
		*addrlen = SUN_LEN(sock_un);
	} else {
		char *port = strtok(NULL, ":");
		if (!port) {
			LOGE("Address format incorrect: failed to parse port");
			ret = EINVAL;
			goto cleanup;
		}
		struct sockaddr_in *sock_in;
		sock_in = (struct sockaddr_in *)addr;
		uint32_t addr_nb;
		sock_in->sin_port = htons(atoi(port));

		if (strcmp(protocol, "inet") == 0) {
			inet_pton(AF_INET, addr_body, &addr_nb);
			sock_in->sin_family = AF_INET;
		} else if (strcmp(protocol, "inet6") == 0) {
			inet_pton(AF_INET6, addr_body, &addr_nb);
			sock_in->sin_family = AF_INET6;
		}
		sock_in->sin_addr.s_addr = addr_nb;
		*addrlen = sizeof(*sock_in);
	}

cleanup:
	free(addr_cpy);
	return ret;
}

int neutron_ctx_set_socket_data_cb(struct neutron_ctx *ctx,
				   neutron_ctx_data_cb cb)
{
	if (!ctx || !cb)
		return EINVAL;

	ctx->data_cb = cb;
	return 0;
}

int neutron_ctx_set_socket_fd_cb(struct neutron_ctx *ctx, neutron_ctx_fd_cb cb)
{
	if (!ctx || !cb)
		return EINVAL;

	ctx->fd_cb = cb;
	return 0;
}

int neutron_ctx_set_socket_event_cb(struct neutron_ctx *ctx,
				    neutron_ctx_event_cb cb)
{
	if (!ctx || !cb)
		return EINVAL;

	ctx->event_cb = cb;
	return 0;
}

struct neutron_conn *neutron_ctx_find_connection(struct neutron_ctx *ctx,
						 int fd)
{
	if (ctx->head->fd == fd)
		return ctx->head;

	struct neutron_conn *aux = ctx->head;
	while (aux->next && aux->next->fd != fd)
		aux = aux->next;

	return aux->next;
}

int neutron_ctx_listen(struct neutron_ctx *ctx,
		       struct sockaddr_storage *addr,
		       ssize_t addrlen)
{
	int ret, opt = 1;

	if (!ctx || !addr)
		return EINVAL;

	ctx->type = NEUTRON_SERVER;

	ctx->socket.addr = addr;
	ctx->socket.addrlen = addrlen;
	ctx->socket.type = addr->ss_family;
	ctx->socket.fd = socket(ctx->socket.type, SOCK_STREAM, 0);

	if (ctx->socket.fd < 0) {
		LOG_ERRNO("Failed to create socket fd");
		return errno;
	}

	if (ctx->fd_cb)
		(*ctx->fd_cb)(ctx, ctx->socket.fd, ctx->userdata);

	ret = setsockopt(
		ctx->socket.fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ret) {
		LOG_ERRNO("Failed to setsockopt");
		return ret;
	}

	ret = bind(ctx->socket.fd,
		   (struct sockaddr *)ctx->socket.addr,
		   ctx->socket.addrlen);
	if (ret) {
		LOG_ERRNO("Failed to bind socket");
		return ret;
	}

	ret = listen(ctx->socket.fd, MAX_SERVER_CONNECTIONS);
	if (ret) {
		LOG_ERRNO("Failed to start listening");
		return ret;
	}

	ret = neutron_loop_add(ctx->loop,
			       ctx->socket.fd,
			       listen_cb,
			       NEUTRON_FD_EVENT_IN,
			       (void *)ctx);

	if (ret) {
		LOG_ERRNO("Failed to register socket fd in event loop");
		return ret;
	}

	return 0;
}

void listen_cb(int server_fd, uint32_t revents, void *userdata)
{
	server_accept_conn(server_fd, userdata);
}

int neutron_ctx_connect(struct neutron_ctx *ctx,
			struct sockaddr_storage *addr,
			ssize_t addrlen)
{
	int ret = 0;

	if (!ctx) {
		LOGE("Failure: ctx is null");
		return EINVAL;
	}
	ctx->type = NEUTRON_CLIENT;

	if (!addr) {
		LOGE("Failure: cannot connect to null address");
		return EINVAL;
	}
	ctx->socket.addr = (struct sockaddr_storage *)addr;
	ctx->socket.addrlen = addrlen;
	ctx->socket.type = addr->ss_family;

	ctx->socket.fd = socket(ctx->socket.type, SOCK_STREAM, 0);
	if (ctx->socket.fd < 0) {
		LOG_ERRNO("Failed to create client socket");
		return errno;
	}

	if (ctx->fd_cb) {
		(*ctx->fd_cb)(ctx, ctx->socket.fd, ctx->userdata);
	}

	ret = connect(ctx->socket.fd,
		      (struct sockaddr *)ctx->socket.addr,
		      ctx->socket.addrlen);
	if (ret < 0) {
		LOG_ERRNO("Failed to connect node to server");
		return errno;
	}

	struct neutron_conn *conn = neutron_conn_new(512);
	conn->fd = ctx->socket.fd;
	conn->remove = 0;
	neutron_ctx_add_conn(ctx, conn);

	ret = neutron_loop_add(ctx->loop,
			       ctx->socket.fd,
			       conn_cb,
			       NEUTRON_FD_EVENT_IN,
			       (void *)ctx);
	if (ret) {
		LOG_ERRNO("Failed to add client socket fd to loop");
		return errno;
	}

	ret = neutron_ctx_notify_event(ctx, NEUTRON_EVENT_CONNECTED, conn);
	if (ret) {
		LOG_ERRNO("Failed to notify connection event");
		return ret;
	}

	return ret;
}

int neutron_ctx_send(struct neutron_ctx *ctx, uint8_t *buf, uint32_t buflen)
{
	int ret = 0;
	int len;
	if (ctx->type == NEUTRON_CLIENT) {
		len = send(ctx->socket.fd, buf, buflen, 0);
		ret = len == buflen ? 0 : errno;
	} else if (ctx->type == NEUTRON_SERVER) {
		struct neutron_conn *aux = ctx->head;
		while (aux) {
			len = send(aux->fd, buf, buflen, 0);
			ret = len == buflen ? 0 : errno;
			if (ret != 0) {
				LOGE("Failed to send the entire buffer to connection fd: %d",
				     aux->fd);
				return ret;
			}
			aux = aux->next;
		}
	}
	return ret;
}

int neutron_ctx_bind(struct neutron_ctx *ctx,
		     struct sockaddr *addr,
		     ssize_t addrlen)
{
	int ret = 0;
	int opt = 1;

	if (!ctx) {
		LOGE("Failure: ctx is null");
		return EINVAL;
	}

	ctx->type = NEUTRON_DGRAM;

	if (!addr) {
		LOGE("Failure: addr is null");
		return EINVAL;
	}

	ctx->socket.fd = socket(addr->sa_family, SOCK_DGRAM, 0);
	if (ctx->socket.fd < 0) {
		LOG_ERRNO("Failed to create socket fd");
		return errno;
	}

	ret = setsockopt(ctx->socket.fd,
			 SOL_SOCKET,
			 SO_REUSEADDR | SO_REUSEPORT,
			 &opt,
			 sizeof(opt));
	if (ret) {
		LOG_ERRNO("Failed to setsockopt");
		return ret;
	}

	if (ctx->fd_cb)
		(*ctx->fd_cb)(ctx, ctx->socket.fd, ctx->userdata);

	ret = bind(ctx->socket.fd, addr, addrlen);
	if (ret < 0) {
		LOG_ERRNO("Bind failed on udp socket");
		return errno;
	}

	ctx->socket.addr = (struct sockaddr_storage *)addr;
	ctx->socket.addrlen = addrlen;

	struct neutron_conn *conn = neutron_conn_new(512);
	conn->fd = ctx->socket.fd;
	conn->remove = 0;

	conn->local = ctx->socket.addr;
	conn->local_addlren = ctx->socket.addrlen;
	neutron_ctx_add_conn(ctx, conn);

	ret = neutron_loop_add(ctx->loop,
			       ctx->socket.fd,
			       conn_cb,
			       NEUTRON_FD_EVENT_IN,
			       (void *)ctx);
	if (ret) {
		LOG_ERRNO("Failed to add client socket fd to loop");
		return errno;
	}

	ret = neutron_ctx_notify_event(ctx, NEUTRON_EVENT_CONNECTED, conn);
	if (ret) {
		LOG_ERRNO("Failed to notify connection event");
		return ret;
	}

	return ret;
}

int neutron_ctx_broadcast(struct neutron_ctx *ctx)
{
	int ret = 0;
	int opt = 1;

	if (!ctx) {
		LOGE("Failure: ctx is null");
		return EINVAL;
	}

	ctx->type = NEUTRON_DGRAM;
	ctx->socket.fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (ctx->socket.fd < 0) {
		LOG_ERRNO("Failed to create socket fd");
		return errno;
	}

	ret = setsockopt(
		ctx->socket.fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
	if (ret) {
		LOG_ERRNO("Failed to setsockopt");
		return ret;
	}

	if (ctx->fd_cb)
		(*ctx->fd_cb)(ctx, ctx->socket.fd, ctx->userdata);

	struct neutron_conn *conn = neutron_conn_new(512);
	conn->fd = ctx->socket.fd;
	conn->remove = 0;

	conn->local = ctx->socket.addr;
	conn->local_addlren = ctx->socket.addrlen;
	neutron_ctx_add_conn(ctx, conn);

	ret = neutron_loop_add(ctx->loop,
			       ctx->socket.fd,
			       conn_cb,
			       NEUTRON_FD_EVENT_IN,
			       (void *)ctx);
	if (ret) {
		LOG_ERRNO("Failed to add client socket fd to loop");
		return ret;
	}

	return 0;
}

int neutron_ctx_send_to(struct neutron_ctx *ctx,
			const struct sockaddr *addr,
			ssize_t addrlen,
			uint8_t *buf,
			uint32_t buflen)
{
	int datalen = sendto(ctx->socket.fd, buf, buflen, 0, addr, addrlen);
	return datalen > 0 ? 0 : errno;
}

int neutron_ctx_disconnect(struct neutron_ctx *ctx)
{
	int ret = 0;
	VLOGE("neutron_ctx_notify_event");
	ret = neutron_ctx_notify_event(
		ctx, NEUTRON_EVENT_DISCONNECTED, ctx->head);

	if (ret) {
		LOG_ERRNO("Failed to notify disconneccted event");
	}

	struct neutron_conn *aux, *next;
	aux = ctx->head;
	while (aux) {
		next = aux->next;
		neutron_ctx_remove_conn(ctx, aux);
		aux = next;
	}

	if (ctx->socket.fd > 0) {
		ret = neutron_loop_remove(ctx->loop, ctx->socket.fd);
		if (ret) {
			LOG_ERRNO("Failed to remove ctx fd from loop");
			return ret;
		}

		ret = close(ctx->socket.fd);
	}

	LOGD("Socked fd(%d) already closed", ctx->socket.fd);
	return 0;
}

int neutron_ctx_remove_conn(struct neutron_ctx *ctx, struct neutron_conn *conn)
{
	int ret = 0;
	uint8_t found = 0;

	struct neutron_conn *aux = ctx->head;
	if (aux == conn) {
		found = 1;
		ctx->head = ctx->head->next;
	} else {
		while (aux->next && aux->next != conn)
			aux = aux->next;
		if (aux->next == conn) {
			found = 1;
			aux->next = aux->next->next;
		} else {
			LOGE("Failed to find conn in ctx");
			return EINVAL;
		}
	}

	if (found) {
		ret = neutron_ctx_notify_event(
			ctx, NEUTRON_EVENT_DISCONNECTED, conn);
		if (ret) {
			LOG_ERRNO("Failed to notify disconnection event");
		}

		ret = neutron_loop_remove(ctx->loop, conn->fd);
		if (ret) {
			LOG_ERRNO("Failed to remove connection fd from loop");
			return ret;
		}
		neutron_conn_destroy(conn);
	}
	return 0;
}

void neutron_ctx_destroy(struct neutron_ctx *ctx)
{
	if (ctx) {
		if (ctx->socket.type == AF_UNIX
		    && ctx->type == NEUTRON_SERVER) {
			unlink(((struct sockaddr_un *)ctx->socket.addr)
				       ->sun_path);
		}

		struct neutron_conn *aux = ctx->head;
		if (ctx->head) {
			ctx->head = ctx->head->next;
			neutron_conn_destroy(aux);
		}

		ctx->socket.addr = NULL;
		ctx->socket.fd = 0;
		ctx->socket.addrlen = 0;

		if (ctx->destroy_loop)
			neutron_loop_destroy(ctx->loop);

		free(ctx);
	}
	ctx = NULL;
}
