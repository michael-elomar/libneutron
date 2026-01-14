#include <priv.h>
#include <node.h>
#include <loop.h>

struct neutron_node *neutron_node_create_with_loop(struct neutron_loop *loop,
						   char *address)
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

	node->destroy_loop = 0;
	return node;

cleanup:
	neutron_node_destroy(node);
	return NULL;
}

struct neutron_node *neutron_node_create(char *address)
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
		neutron_node_create_with_loop(loop, address);

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

void neutron_node_destroy(struct neutron_node *node)
{
	if (node) {
		if (node->socket.local_addr != NULL) {
			free(node->socket.local_addr);
			node->socket.local_addr = NULL;
		}
		node->socket.fd = 0;
		node->socket.local_addrlen = 0;

		if (node->destroy_loop)
			neutron_loop_destroy(node->loop);

		free(node);
	}
	node = NULL;
}
