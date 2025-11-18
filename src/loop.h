#ifndef _LOOP_H_
#define _LOOP_H_

#include <syskit.h>

struct syskit_fd {
	intptr_t fd;
	uint32_t events;
	syskit_fd_event_cb cb;

	struct syskit_fd *next;
};

struct syskit_loop {
	struct syskit_fd *sfd; /* Linked list of FDs tracked by the loop */
	intptr_t number_fds;

	uint32_t flags;
	intptr_t efd;
};

#endif // ! _LOOP_H_
