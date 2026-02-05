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
struct neutron_timer;

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
	NEUTRON_DGRAM,
};

enum neutron_event {
	NEUTRON_EVENT_CONNECTED = 0,
	NEUTRON_EVENT_DISCONNECTED,
	NEUTRON_EVENT_DATA,
};

typedef void (*neutron_fd_event_cb)(int fd, uint32_t revents, void *userdata);

typedef void (*neutron_ctx_fd_cb)(struct neutron_ctx *ctx,
				  int fd,
				  void *userdata);

typedef void (*neutron_ctx_event_cb)(struct neutron_ctx *ctx,
				     enum neutron_event event,
				     struct neutron_conn *conn,
				     void *userdata);

typedef void (*neutron_ctx_data_cb)(struct neutron_ctx *ctx,
				    struct neutron_conn *conn,
				    uint8_t *buf,
				    uint32_t buflen,
				    void *userdata);

typedef void (*neutron_evt_cb)(struct neutron_evt *evt, void *userdata);

typedef void (*neutron_timer_cb)(struct neutron_timer *timer, void *userdata);

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

/* ctx public API */

struct neutron_ctx *neutron_ctx_create(neutron_ctx_event_cb cb, void *userdata);

struct neutron_ctx *neutron_ctx_create_with_loop(neutron_ctx_event_cb cb,
						 struct neutron_loop *loop,
						 void *userdata);

struct neutron_loop *neutron_ctx_get_loop(struct neutron_ctx *ctx);

int neutron_ctx_parse_address(const char *address,
			      struct sockaddr_storage *addr,
			      socklen_t *addrlen);

int neutron_ctx_set_socket_data_cb(struct neutron_ctx *ctx,
				   neutron_ctx_data_cb cb);

int neutron_ctx_set_socket_fd_cb(struct neutron_ctx *ctx, neutron_ctx_fd_cb cb);

int neutron_ctx_set_socket_event_cb(struct neutron_ctx *ctx,
				    neutron_ctx_event_cb cb);

int neutron_ctx_listen(struct neutron_ctx *ctx,
		       struct sockaddr_storage *addr,
		       ssize_t addrlen);

int neutron_ctx_connect(struct neutron_ctx *ctx,
			struct sockaddr_storage *addr,
			ssize_t addrlen);

int neutron_ctx_disconnect(struct neutron_ctx *ctx);

int neutron_ctx_send(struct neutron_ctx *ctx, uint8_t *buf, uint32_t buflen);

int neutron_ctx_bind(struct neutron_ctx *ctx,
		     struct sockaddr_storage *addr,
		     ssize_t addrlen);

int neutron_ctx_broadcast(struct neutron_ctx *ctx);

int neutron_ctx_send_to(struct neutron_ctx *ctx,
			struct sockaddr_storage *addr,
			ssize_t addrlen,
			uint8_t *buf,
			uint32_t buflen);

void neutron_ctx_destroy(struct neutron_ctx *ctx);

/* event public API */

struct neutron_evt *neutron_evt_create(int flags, neutron_evt_cb cb);

void neutron_evt_destroy(struct neutron_evt *evt);

int neutron_evt_attach(struct neutron_evt *evt, struct neutron_loop *loop);

int neutron_evt_detach(struct neutron_evt *evt, struct neutron_loop *loop);

int neutron_evt_trigger(struct neutron_evt *evt);

int neutron_evt_clear(struct neutron_evt *evt);

/* timer public API */

struct neutron_timer *neutron_timer_create(struct neutron_loop *loop,
					   neutron_timer_cb cb,
					   void *userdata);

void neutron_timer_destroy(struct neutron_timer *timer);

int neutron_timer_set_periodic(struct neutron_timer *timer,
			       uint32_t delay,
			       uint32_t period);

int neutron_timer_set(struct neutron_timer *timer, uint32_t delay);

int neutron_timer_clear(struct neutron_timer *timer);

#ifdef __cplusplus
}
#endif
#endif
