#ifndef _NEUTRON_H_
#define _NEUTRON_H_

#include <priv.h>

#ifdef __cplusplus
extern "C" {
#endif

struct neutron_loop;
struct neutron_fd;
struct neutron_evt;
struct neutron_node;

enum neutron_fd_event {
	NEUTRON_FD_EVENT_IN = 0x001,
	NEUTRON_FD_EVENT_PRI = 0x002,
	NEUTRON_FD_EVENT_OUT = 0x004,
	NEUTRON_FD_EVENT_ERROR = 0x008,
	NEUTRON_FD_EVENT_HUP = 0x010,
};

enum neutron_node_type {
	NEUTRON_NODE_SERVER = 0,
	NEUTRON_NODE_CLIENT,
};

typedef void (*neutron_fd_event_cb)(int fd, uint32_t revents, void *userdata);

typedef void (*neutron_socket_fd_cb)(int fd, void *userdata);

/* loop public API */

struct neutron_loop *neutron_loop_create();

int neutron_loop_add(struct neutron_loop *loop,
		     int fd,
		     neutron_fd_event_cb cb,
		     uint32_t events,
		     void *userdata);

int neutron_loop_remove(struct neutron_loop *loop, int fd);

struct neutron_fd *neutron_loop_find_fd(struct neutron_loop *loop, int fd);

int neutron_loop_spin(struct neutron_loop *loop);

void neutron_loop_destroy(struct neutron_loop *loop);

void neutron_loop_wakeup(struct neutron_loop *loop);

void neutron_loop_display_registered_fds(struct neutron_loop *loop);

/* node public API */

struct neutron_node *neutron_node_create();

struct neutron_node *neutron_node_create_with_loop(struct neutron_loop *loop);

struct sockaddr_storage *neutron_node_parse_address(char *address);

void neutron_node_destroy(struct neutron_node *node);

/* event public API */

struct neutron_evt *neutron_evt_create();

void neutron_evt_destroy(struct neutron_evt *evt);

#ifdef __cplusplus
}
#endif
#endif
