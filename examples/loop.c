#include <neutron.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

int running = 1;
struct neutron_loop *loop;

void sig_handler(int signum)
{
	if (loop)
		neutron_loop_wakeup(loop);
	running = 0;
}

void callback(int fd, uint32_t revents, void *userdata)
{
	uint8_t buf[128];
	read(fd, buf, 128);
	LOGD("Inside %s, events: %d, %s", __func__, revents, buf);
}

int main(int argc, char *argv[])
{
	/* exit on SIGINT & SIGTERM */
	signal(SIGINT, &sig_handler);
	signal(SIGTERM, &sig_handler);

	/* ignore SIGPIPE */
	signal(SIGPIPE, SIG_IGN);
	loop = neutron_loop_create();

	int fd = STDIN_FILENO;

	if (fd < 0) {
		perror("open");
		return -errno;
	}

	neutron_loop_add(loop, fd, callback, NEUTRON_FD_EVENT_IN, NULL);

	while (running) {
		neutron_loop_spin(loop);
	}

	close(fd);
	neutron_loop_destroy(loop);

	return 0;
}
