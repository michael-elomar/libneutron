#include <priv.h>
#include <node.h>
#include <loop.h>

static void neutron_node_add_conn(struct neutron_node *node,
				  struct neutron_conn *conn)
{
	if (!node->head) {
		node->head = conn;
		return;
	}

	struct neutron_conn *aux = node->head;
	while (aux->next)
		aux = aux->next;

	aux->next = conn;
}
static void conn_process_write(struct neutron_node *node,
			       struct neutron_conn *conn)
{
	/* TODO */
}

static void conn_process_read(struct neutron_node *node,
			      struct neutron_conn *conn)
{
	do {
		conn->readbuf.datalen = read(
			conn->fd, conn->readbuf.data, conn->readbuf.capacity);
	} while (conn->readbuf.datalen < 0 && errno != EINTR);

	if (node->socket_data_cb) {
		(*node->socket_data_cb)(
			conn->fd, conn->readbuf.data, conn->readbuf.datalen);
	}

	conn->remove = 1;
}

static void conn_accepted_cb(int fd, uint32_t revents, void *userdata)
{
	struct neutron_node *node = (struct neutron_node *)userdata;
	struct neutron_conn *conn = neutron_node_find_connection(node, fd);

	if (!conn->remove && (revents & NEUTRON_FD_EVENT_IN))
		conn_process_read(node, conn);
	else if (!conn->remove && (revents & NEUTRON_FD_EVENT_OUT))
		conn_process_write(node, conn);
	else if (conn->remove || NEUTRON_FD_EVENT_ERROR)
		neutron_node_remove_conn(node, conn);
}

static int node_accept_conn(int server_fd, struct neutron_node *node)
{
	int ret = 0, conn_fd = -1;

	conn_fd = accept(server_fd,
			 (struct sockaddr *)node->socket.local_addr,
			 &node->socket.local_addrlen);

	if (conn_fd < 0) {
		ret = errno;
		LOG_ERRNO("Failed to accept connection");
		goto cleanup;
	}

	struct neutron_conn *conn = neutron_conn_new(512);
	conn->fd = conn_fd;
	neutron_node_add_conn(node, conn);

	ret = neutron_loop_add(node->loop,
			       conn_fd,
			       conn_accepted_cb,
			       NEUTRON_FD_EVENT_IN,
			       node);

	if (ret) {
		LOG_ERRNO("Failed to add connection fd to event loop");
		goto cleanup;
	}

	return 0;

cleanup:
	if (conn_fd > 0)
		close(conn_fd);

	return ret;
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

	return conn;
}

void neutron_conn_destroy(struct neutron_conn *conn)
{
	if (conn) {
		if (conn->readbuf.data)
			free(conn->readbuf.data);

		if (conn->fd > 0)
			close(conn->fd);

		conn->fd = -1;
		conn->readbuf.data = NULL;
		conn->readbuf.capacity = 0;
		conn->readbuf.datalen = 0;

		free(conn);
		conn = NULL;
	}
}

struct neutron_node *neutron_node_create_with_loop(struct neutron_loop *loop,
						   char *address,
						   void *userdata)
{
	if (!address) {
		LOGE("Must provide an address to the node");
		return NULL;
	}

	struct neutron_node *node =
		(struct neutron_node *)calloc(1, sizeof(struct neutron_node));

	if (!node) {
		LOG_ERRNO("Failed to allocate memory for new node");
		goto cleanup;
	}

	node->loop = loop;

	node->socket.local_addr = neutron_node_parse_address(address);
	node->socket.local_addrlen = sizeof(struct sockaddr_storage);

	node->socket.fd =
		socket(node->socket.local_addr->ss_family, SOCK_STREAM, 0);

	if (node->socket.fd < 0) {
		LOG_ERRNO("Failed to create socket fd");
		goto cleanup;
	}

	node->userdata = userdata;

	if (node->socket_fd_cb)
		(*node->socket_fd_cb)(node->socket.fd, node->userdata);

	node->destroy_loop = 0;
	return node;

cleanup:
	neutron_node_destroy(node);
	return NULL;
}

struct neutron_node *neutron_node_create(char *address, void *userdata)
{
	if (!address) {
		LOGE("Must provide an address to the node");
		return NULL;
	}

	struct neutron_loop *loop = neutron_loop_create();

	if (!loop) {
		LOGE("Failed to create neutron_loop for node");
		goto cleanup;
	}

	struct neutron_node *node =
		neutron_node_create_with_loop(loop, address, userdata);

	if (!node) {
		LOGE("Failed to create neutron_node");
		goto cleanup;
	}

	node->destroy_loop = 1;

	return node;

cleanup:
	neutron_node_destroy(node);
	neutron_loop_destroy(loop);
	return NULL;
}

struct sockaddr_storage *neutron_node_parse_address(char *address)
{
	if (strlen(address) > 64)
		return NULL;

	struct sockaddr_storage *storage = NULL;

	char *addr_cpy = strdup(address);
	char *protocol = strtok(addr_cpy, ":");
	if (!protocol) {
		LOGE("Address format incorrect: failed to parse protocol");
		goto cleanup;
	}

	char *addr_body = strtok(NULL, ":");
	if (!addr_body) {
		LOGE("Address format incorrect: failed to parse address");
		goto cleanup;
	}

	if (strcmp(protocol, "unix") == 0) {
		struct sockaddr_un *sock_un;
		sock_un = (struct sockaddr_un *)calloc(
			1, sizeof(struct sockaddr_un));
		if (!sock_un) {
			goto cleanup;
		}
		sock_un->sun_family = AF_UNIX;
		strcpy(sock_un->sun_path, addr_body);
		storage = (struct sockaddr_storage *)sock_un;

	} else {
		char *port = strtok(NULL, ":");
		if (!port) {
			LOGE("Address format incorrect: failed to parse port");
			goto cleanup;
		}
		struct sockaddr_in *sock_in;
		sock_in = (struct sockaddr_in *)calloc(
			1, sizeof(struct sockaddr_in));
		if (!sock_in) {
			goto cleanup;
		}
		uint32_t addr_nb;
		sock_in->sin_port = htons(atoi(port));

		if (strcmp(protocol, "inet") == 0) {
			inet_pton(AF_INET, addr_body, &addr_nb);
			sock_in->sin_family = AF_INET;
			storage = (struct sockaddr_storage *)&sock_in;
		} else if (strcmp(protocol, "inet6") == 0) {
			inet_pton(AF_INET6, addr_body, &addr_nb);
			sock_in->sin_family = AF_INET6;
		}
		sock_in->sin_addr.s_addr = addr_nb;
		storage = (struct sockaddr_storage *)sock_in;
	}

cleanup:
	free(addr_cpy);
	return storage;
}

int neutron_node_set_socket_data_cb(struct neutron_node *node,
				    neutron_socket_data_cb cb)
{
	if (!node || !cb)
		return EINVAL;

	node->socket_data_cb = cb;
	return 0;
}

int neutron_node_set_socket_fd_cb(struct neutron_node *node,
				  neutron_socket_fd_cb cb)
{
	if (!node || !cb)
		return EINVAL;

	node->socket_fd_cb = cb;
	return 0;
}

int neutron_node_set_socket_event_cb(struct neutron_node *node,
				     neutron_socket_event_cb cb)
{
	if (!node || !cb)
		return EINVAL;

	node->socket_event_cb = cb;
	return 0;
}

struct neutron_conn *neutron_node_find_connection(struct neutron_node *node,
						  int fd)
{
	if (node->head->fd == fd)
		return node->head;

	struct neutron_conn *aux = node->head;
	while (aux->next && aux->next->fd != fd)
		aux = aux->next;

	return aux->next;
}

int neutron_node_listen(struct neutron_node *node)
{
	int ret, opt = 1;
	ret = setsockopt(node->socket.fd,
			 SOL_SOCKET,
			 SO_REUSEADDR | SO_REUSEPORT,
			 &opt,
			 sizeof(opt));
	if (ret) {
		LOG_ERRNO("Failed to setsockopt");
		return ret;
	}

	ret = bind(node->socket.fd,
		   (struct sockaddr *)node->socket.local_addr,
		   node->socket.local_addrlen);
	if (ret) {
		LOG_ERRNO("Failed to bind socket");
		return ret;
	}

	ret = listen(node->socket.fd, MAX_SERVER_CONNECTIONS);
	if (ret) {
		LOG_ERRNO("Failed to start listening");
		return ret;
	}

	ret = neutron_loop_add(node->loop,
			       node->socket.fd,
			       listen_cb,
			       NEUTRON_FD_EVENT_IN,
			       (void *)node);

	if (ret) {
		LOG_ERRNO("Failed to register socket fd in event loop");
		return ret;
	}

	node->type = NEUTRON_NODE_SERVER;

	return 0;
}

void listen_cb(int server_fd, uint32_t revents, void *userdata)
{
	node_accept_conn(server_fd, userdata);
}

int neutron_node_connect(struct neutron_node *node)
{
	// TODO
	return 0;
}

int neutron_node_remove_conn(struct neutron_node *node,
			     struct neutron_conn *conn)
{
	int ret = 0;
	uint8_t found = 0;

	struct neutron_conn *aux = node->head;
	if (aux == conn) {
		found = 1;
		node->head = node->head->next;
		ret = neutron_loop_remove(node->loop, conn->fd);
		if (ret) {
			LOG_ERRNO("Failed to remove connection fd from loop");
			return ret;
		}
		neutron_conn_destroy(conn);
	} else {
		while (aux->next && aux->next != conn)
			aux = aux->next;
		if (aux->next == conn) {
			found = 1;
			aux->next = aux->next->next;
			ret = neutron_loop_remove(node->loop, conn->fd);
			if (ret) {
				LOG_ERRNO(
					"Failed to remove connection fd from loop");
				return ret;
			}
			neutron_conn_destroy(conn);
		} else {
			LOGE("Failed to find conn in node");
			return EINVAL;
		}
	}
	return 0;
}

void neutron_node_destroy(struct neutron_node *node)
{
	if (node) {
		if (node->socket.local_addr != NULL) {
			free(node->socket.local_addr);
			node->socket.local_addr = NULL;
		}

		if (node->socket.fd > 0)
			close(node->socket.fd);

		node->socket.fd = 0;
		node->socket.local_addrlen = 0;

		/* clear linked list */
		struct neutron_conn *aux = node->head;
		if (node->head) {
			struct neutron_conn *aux = node->head;
			node->head = node->head->next;
			neutron_conn_destroy(aux);
		}

		if (node->destroy_loop)
			neutron_loop_destroy(node->loop);

		free(node);
	}
	node = NULL;
}
