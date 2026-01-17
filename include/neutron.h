#ifndef _NEUTRON_H_
#define _NEUTRON_H_

#include <priv.h>

#ifdef __cplusplus
extern "C" {
#endif

struct neutron_loop;
struct neutron_fd;
struct neutron_evt;
struct neutron_ctx;
struct neutron_conn;

enum neutron_fd_event {
	NEUTRON_FD_EVENT_IN = 0x001,
	NEUTRON_FD_EVENT_PRI = 0x002,
	NEUTRON_FD_EVENT_OUT = 0x004,
	NEUTRON_FD_EVENT_ERROR = 0x008,
	NEUTRON_FD_EVENT_HUP = 0x010,
};

enum neutron_ctx_type {
	NEUTRON_SERVER = 0,
	NEUTRON_CLIENT,
};

enum neutron_event {
	NEUTRON_EVENT_CONNECTED = 0,
	NEUTRON_EVENT_DISCONNECTED,
	NEUTRON_EVENT_DATA,
};

typedef void (*neutron_fd_event_cb)(int fd, uint32_t revents, void *userdata);

typedef void (*neutron_ctx_fd_cb)(int fd, void *userdata);

typedef void (*neutron_ctx_event_cb)(struct neutron_ctx *ctx,
				     enum neutron_event event,
				     struct neutron_conn *conn,
				     void *userdata);

typedef void (*neutron_ctx_data_cb)(int conn_fd, void *buf, uint32_t buflen);

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

struct neutron_ctx *neutron_ctx_create(neutron_ctx_event_cb cb, void *userdata);

struct neutron_ctx *neutron_ctx_create_with_loop(neutron_ctx_event_cb cb,
						 struct neutron_loop *loop,
						 void *userdata);

struct sockaddr_storage *neutron_ctx_parse_address(char *address);

int neutron_ctx_set_socket_data_cb(struct neutron_ctx *ctx,
				   neutron_ctx_data_cb cb);

int neutron_ctx_set_socket_fd_cb(struct neutron_ctx *ctx, neutron_ctx_fd_cb cb);

int neutron_ctx_set_socket_event_cb(struct neutron_ctx *ctx,
				    neutron_ctx_event_cb cb);

int neutron_ctx_listen(struct neutron_ctx *ctx,
		       struct sockaddr *addr,
		       ssize_t addrlen);

int neutron_ctx_connect(struct neutron_ctx *ctx,
			struct sockaddr *addr,
			ssize_t addrlen);

int neutron_ctx_disconnect(struct neutron_ctx *ctx);

int neutron_ctx_send(struct neutron_ctx *ctx, uint8_t *buf, uint32_t buflen);

void neutron_ctx_destroy(struct neutron_ctx *ctx);

/* event public API */

struct neutron_evt *neutron_evt_create();

void neutron_evt_destroy(struct neutron_evt *evt);

#ifdef __cplusplus
}
#endif
#endif
