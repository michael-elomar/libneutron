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

#endif /* _TIMER_H_ */
