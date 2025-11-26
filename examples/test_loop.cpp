#include <syskit.hpp>
#include <signal.h>

int running = 1;
SyskitLoop *loop;

void sighandler(int signum)
{
	running = 0;
	if (loop != nullptr)
		loop->wakeup();
}

void callback(int fd, uint32_t revents, void *userdata)
{
	uint8_t buf[128];
	read(fd, buf, 128);
	LOGD("Inside %s, events: %d, %s", __func__, revents, buf);
}

int main(int argc, char **argv)
{
	/* exit on SIGINT & SIGTERM */
	signal(SIGINT, &sighandler);
	signal(SIGTERM, &sighandler);

	/* ignore SIGPIPE */
	signal(SIGPIPE, SIG_IGN);

	loop = new SyskitLoop();

	int fd = STDIN_FILENO;

	if (fd < 0) {
		perror("open");
		return -errno;
	}

	loop->add(fd, callback, SYSKIT_FD_EVENT_IN, nullptr);

	struct syskit_fd *loop_fd = loop->findFd(fd);

	while (running) {
		loop->spin();
	}

	close(fd);
	delete loop;

	return 0;
}
