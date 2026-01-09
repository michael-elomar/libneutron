#include <neutron.h>
/* Loop wrapping in C++ */

class NeutronLoop {
public:
	NeutronLoop()
	{
		mLoop = neutron_loop_create();
	}

	~NeutronLoop()
	{
		neutron_loop_destroy(mLoop);
	}

	int add(int fd, neutron_fd_event_cb cb, uint32_t events, void *userdata)
	{
		return neutron_loop_add(mLoop, fd, cb, events, userdata);
	}

	struct neutron_fd *findFd(int fd)
	{
		return neutron_loop_find_fd(mLoop, fd);
	}

	int remove(int fd)
	{
		return neutron_loop_remove(mLoop, fd);
	}

	int spin()
	{
		return neutron_loop_spin(mLoop);
	}

	void wakeup()
	{
		neutron_loop_wakeup(mLoop);
	}

private:
	struct neutron_loop *mLoop;
};
