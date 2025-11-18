#ifndef _LOOP_H_
#define _LOOP_H_

#include <priv.h>

struct syskit_fd {
	intptr_t fd;
	uint32_t events;
};

void test_function(void);

#endif // ! _LOOP_H_


