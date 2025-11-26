#include <syskit.h>
/* Loop wrapping in C++ */

class SyskitLoop {
public:
	SyskitLoop()
	{
		mLoop = syskit_loop_create();
	}

	~SyskitLoop()
	{
		syskit_loop_destroy(mLoop);
	}

	int add(int fd, syskit_fd_event_cb cb, uint32_t events, void *userdata)
	{
		return syskit_loop_add(mLoop, fd, cb, events, userdata);
	}

	struct syskit_fd *findFd(int fd)
	{
		return syskit_loop_find_fd(mLoop, fd);
	}

	int remove(int fd)
	{
		return syskit_loop_remove(mLoop, fd);
	}

	int spin()
	{
		return syskit_loop_spin(mLoop);
	}

	void wakeup()
	{
		syskit_loop_wakeup(mLoop);
	}

private:
	struct syskit_loop *mLoop;
};
