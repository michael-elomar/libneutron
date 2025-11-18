#include <syskit.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

int running = 1;

void sig_handler(int signum)
{
	LOGI("Received signal=%s", strsignal(signum));
	running = 0;
}

void callback(int fd, uint32_t revents, void *userdata)
{
	LOGD("Inside %s, events: %d", __func__, revents);
}

int main(int argc, char *argv[])
{
	/* exit on SIGINT & SIGTERM */
	signal(SIGINT, &sig_handler);
	signal(SIGTERM, &sig_handler);

	/* ignore SIGPIPE */
	signal(SIGPIPE, SIG_IGN);

	struct syskit_loop *loop = syskit_loop_create();

	int fd = open("/dev/input/mouse0", O_RDONLY);
	if (fd < 0) {
		perror("open");
		return -errno;
	}

	syskit_loop_add(loop, fd, callback, SYSKIT_FD_EVENT_IN);

	while (running) {
		syskit_loop_spin(loop);
		LOGI("Here");
	}

	close(fd);
	syskit_loop_destroy(loop);

	return 0;
}
