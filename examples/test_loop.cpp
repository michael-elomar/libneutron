#include <neutron.hpp>
#include <signal.h>

int running = 1;
neutron::Loop *loop;

void sighandler(int signum)
{
	running = 0;
	if (loop != nullptr)
		loop->wakeup();
}

class LoopHandler : public neutron::Loop::Handler {
	void processEvent(int fd, uint32_t revents) override
	{
		uint8_t buf[128];
		memset(buf, '\0', 128);
		read(fd, buf, 128);
		LOGD("Inside %s, events: %d, %s", __func__, revents, buf);
	}
};

int main(int argc, char **argv)
{
	/* exit on SIGINT & SIGTERM */
	signal(SIGINT, &sighandler);
	signal(SIGTERM, &sighandler);

	/* ignore SIGPIPE */
	signal(SIGPIPE, SIG_IGN);

	loop = new neutron::Loop();

	int fd = STDIN_FILENO;

	if (fd < 0) {
		perror("open");
		return -errno;
	}

	LoopHandler *handler = new LoopHandler();

	loop->add(fd, NEUTRON_FD_EVENT_IN, handler);

	struct neutron_fd *loop_fd = loop->findFd(fd);

	while (running) {
		loop->spin();
	}

	close(fd);
	delete loop;

	return 0;
}
