#ifndef _EVT_H_
#define _EVT_H_

#define NEUTRON_EVT_MAGIC 0xBEEF

#include <neutron.h>

struct neutron_evt {
	struct neutron_loop *loop;

	neutron_evt_cb cb;

	int fd;

	void *userdata;
};

#endif
