#include <syskit.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

int running = 1;
struct syskit_loop *loop;

void sig_handler(int signum)
{
	syskit_loop_wakeup(loop);
	running = 0;
	LOGI("Received signal=%s", strsignal(signum));
	LOGI("Running status: %d", running);
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
	loop = syskit_loop_create();

	int fd = STDIN_FILENO;

	if (fd < 0) {
		perror("open");
		return -errno;
	}

	syskit_loop_add(loop, fd, callback, SYSKIT_FD_EVENT_IN);

	while (running) {
		syskit_loop_spin(loop);
	}

	close(fd);
	syskit_loop_destroy(loop);

	return 0;
}
