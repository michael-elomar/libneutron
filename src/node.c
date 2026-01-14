#include <priv.h>
#include <node.h>
#include <loop.h>

struct neutron_node *neutro_loop_create_with_loop(struct neutron_loop *loop)
{
	struct neutron_node *node =
		(struct neutron_node *)calloc(1, sizeof(struct neutron_node));

	if (!node) {
		LOGE("Failed to create neutron_node");
		return NULL;
	}

	node->loop = loop;

	return node;
}

struct neutron_node *neutron_node_create()
{
	struct neutron_loop *loop =
		(struct neutron_loop *)calloc(1, sizeof(struct neutron_loop));

	if (!loop) {
		LOGE("Failed to create neutron_loop for node");
		return NULL;
	}
	struct neutron_node *node = neutron_node_create_with_loop(loop);

	if (!node)
		return NULL;

	return node;
}
