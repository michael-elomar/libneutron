#ifndef _LOOP_H_
#define _LOOP_H_

#include <syskit.h>

#define LOOP_WAKEUP_MAGIC 0x35
#define MAX_EVENTS 16

struct syskit_fd {
	intptr_t fd;
	uint32_t events;
	void *userdata;
	syskit_fd_event_cb cb;

	struct syskit_fd *next;
};

struct syskit_loop {
	struct syskit_fd *sfd; /* Linked list of FDs tracked by the loop */
	intptr_t number_fds;

	uint32_t flags;
	intptr_t efd; /* fd associate with the epoll context of the loop */

	/* eventfd that will be used to force the loop to wakeup */
	eventfd_t wakeup_fd;
};

#endif // ! _LOOP_H_
