#include <priv.h>
#include <node.h>
#include <loop.h>

static void conn_accepted_cb(int fd, uint32_t revents, void *userdata)
{
	ssize_t readlen = 0;
	struct neutron_node *node = (struct neutron_node *)userdata;

	// TODO: spawn connection object and have it read from the connection
	// file descriptor and keep track of all connection objects in a list
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
	struct neutron_node *node = (struct neutron_node *)userdata;

	node_accept_conn(server_fd, node);
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

		if (node->destroy_loop)
			neutron_loop_destroy(node->loop);

		free(node);
	}
	node = NULL;
}
