#include <priv.h>
#include <node.h>
#include <loop.h>

struct neutron_node *neutron_node_create_with_loop(struct neutron_loop *loop)
{
	struct neutron_node *node =
		(struct neutron_node *)calloc(1, sizeof(struct neutron_node));

	if (!node) {
		LOG_ERRNO("Failed to allocate memory for new node");
		return NULL;
	}

	node->loop = loop;

	return node;
}

struct neutron_node *neutron_node_create()
{
	struct neutron_loop *loop = neutron_loop_create();

	if (!loop) {
		LOGE("Failed to create neutron_loop for node");
		return NULL;
	}

	struct neutron_node *node = neutron_node_create_with_loop(loop);

	if (!node) {
		LOGE("Failed to create neutron_node");
		return NULL;
	}

	return node;
}

struct sockaddr_storage *neutron_node_parse_address(char *address)
{
	if (strlen(address) > 64)
		return NULL;

	struct sockaddr_storage *storage;

	char *addr_cpy = strdup(address);
	char *protocol = strtok(addr_cpy, ":");
	LOGI("%s", addr_cpy);
	if (protocol == NULL) {
		LOGE("Address format incorrect: failed to parse protocol");
		return NULL;
	}
	char *addr_body = strtok(NULL, ":");
	if (!addr_body) {
		LOGE("Address format incorrect: failed to parse address");
	}
	if (strcmp(protocol, "unix") == 0) {
		struct sockaddr_un sock_un;
		sock_un.sun_family = AF_UNIX;
		strcpy(sock_un.sun_path, addr_body);
		storage = (struct sockaddr_storage *)&sock_un;

	} else {
		char *port = strtok(NULL, ":");
		if (!port) {
			LOGE("Address format incorrect: failed to parse port");
			return NULL;
		}
		struct sockaddr_in sock_in;
		uint32_t addr_nb;
		sock_in.sin_port = htons(atoi(port));

		if (strcmp(protocol, "inet") == 0) {
			inet_pton(AF_INET, addr_body, &addr_nb);
			sock_in.sin_family = AF_INET;
			storage = (struct sockaddr_storage *)&sock_in;
		} else if (strcmp(protocol, "inet6") == 0) {
			inet_pton(AF_INET6, addr_body, &addr_nb);
			sock_in.sin_family = AF_INET6;
		}
		sock_in.sin_addr.s_addr = addr_nb;
		storage = (struct sockaddr_storage *)&sock_in;
	}
	free(addr_cpy);

	return storage;
}

void neutron_node_destroy(struct neutron_node *node)
{
	free(node);
	node = NULL;
}
