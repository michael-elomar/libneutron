#include <priv.h>

#ifdef __cplusplus
extern "C" {
#endif

struct neutron_loop;
struct neutron_fd;
struct neutron_evt;

enum neutron_fd_event {
	NEUTRON_FD_EVENT_IN = 0x001,
	NEUTRON_FD_EVENT_PRI = 0x002,
	NEUTRON_FD_EVENT_OUT = 0x004,
	NEUTRON_FD_EVENT_ERROR = 0x008,
	NEUTRON_FD_EVENT_HUP = 0x010,
};

typedef void (*neutron_fd_event_cb)(int fd, uint32_t revents, void *userdata);

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

/* event public API */

struct neutron_evt *neutron_evt_create();

void neutron_evt_destroy(struct neutron_evt *evt);

#ifdef __cplusplus
}
#endif
