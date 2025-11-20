#include <priv.h>

struct syskit_loop;
struct syskit_fd;
struct syskit_evt;

enum syskit_fd_event {
	SYSKIT_FD_EVENT_IN = 0x001,
	SYSKIT_FD_EVENT_PRI = 0x002,
	SYSKIT_FD_EVENT_OUT = 0x004,
	SYSKIT_FD_EVENT_ERROR = 0x008,
	SYSKIT_FD_EVENT_HUP = 0x010,
};

typedef void (*syskit_fd_event_cb)(int fd, uint32_t revents, void *userdata);

/* loop public API */

struct syskit_loop *syskit_loop_create();

int syskit_loop_add(struct syskit_loop *loop,
		    int fd,
		    syskit_fd_event_cb cb,
		    uint32_t events,
		    void *userdata);

int syskit_loop_remove(struct syskit_loop *loop, int fd);

struct syskit_fd *syskit_loop_find_fd(struct syskit_loop *loop, int fd);

int syskit_loop_spin(struct syskit_loop *loop);

void syskit_loop_destroy(struct syskit_loop *loop);

void syskit_loop_wakeup(struct syskit_loop *loop);

void syskit_loop_display_registered_fds(struct syskit_loop *loop);

/* event public API */

struct syskit_evt *syskit_evt_create();

void syskit_evt_destroy(struct syskit_evt *evt);
