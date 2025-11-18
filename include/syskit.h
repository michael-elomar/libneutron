#include <priv.h>

struct syskit_loop;

enum syskit_fd_event {
	SYSKIT_FD_EVENT_IN = 0x001,
	SYSKIT_FD_EVENT_PRI = 0x002,
	SYSKIT_FD_EVENT_OUT = 0x004,
	SYSKIT_FD_EVENT_ERROR = 0x008,
	SYSKIT_FD_EVENT_HUP = 0x010,
};

typedef void (*syskit_fd_event_cb)(int fd, uint32_t revents, void *userdata);

struct syskit_loop *syskit_loop_create();

int syskit_loop_add(struct syskit_loop *loop, int fd, uint32_t events);

int syskit_loop_remove(struct syskit_loop *loop, int fd);

void syskit_loop_destroy(struct syskit_loop *loop);
