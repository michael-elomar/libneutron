#ifndef _TIMER_H_
#define _TIMER_H_

#include <neutron.h>

struct neutron_timer {
	uint32_t delay, period;

	int tfd; /* Timer fd */

	struct neutron_loop *loop;

	void *userdata;

	neutron_timer_cb cb;
};

int neutron_timer_fd_set(struct neutron_timer *timer,
			 uint32_t delay,
			 uint32_t period);

#endif /* _TIMER_H_ */
