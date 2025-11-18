#include <loop.h>
#include <priv.h>

static void syskit_loop_register_fd(struct syskit_loop *loop,
				    struct syskit_fd *sysfd);

struct syskit_loop *syskit_loop_create()
{
	struct syskit_loop *loop = calloc(1, sizeof(struct syskit_loop));
	if (!loop) {
		LOG_ERRNO("cannot create loop");
		goto error;
	}
	/* init loop flags here */
	loop->flags = 0;

	/* create epoll context */
	loop->efd = epoll_create1(loop->flags);
	if (loop->efd < 0) {
		LOG_ERRNO("cannot create epoll context");
		goto error;
	}

	return loop;

error:
	free(loop);
	return NULL;
}

int syskit_loop_add(struct syskit_loop *loop, int fd, uint32_t events)
{
	int ret = 0;

	struct syskit_fd *sysfd = calloc(1, sizeof(struct syskit_fd));
	if (!sysfd) {
		LOG_ERRNO("cannot allocate memory for syskit_fd instance");
		free(sysfd);
		return errno;
	}
	sysfd->fd = fd;
	sysfd->events = events;

	syskit_loop_register_fd(loop, sysfd);

	struct epoll_event *event = calloc(1, sizeof(struct epoll_event));
	if (!event) {
		LOG_ERRNO("cannot calloc memory for epoll_event");
		free(event);
		return errno;
	}
	event->data.fd = fd;
	event->events = events;

	ret = epoll_ctl(loop->efd, EPOLL_CTL_ADD, fd, event);
	if (ret < 0) {
		LOG_ERRNO("cannot add fd to loop");
		return errno;
	}
	return 0;
}

int syskit_loop_remove(struct syskit_loop *loop, int fd)
{
	int ret = 0;
	struct syskit_fd *head = loop->sfd;

	while (head && head->fd != fd)
		head = head->next;

	return ret;
}

void syskit_loop_destroy(struct syskit_loop *loop)
{
	free(loop);
}

static inline void syskit_loop_register_fd(struct syskit_loop *loop,
					   struct syskit_fd *sysfd)
{
	struct syskit_fd *head = loop->sfd;
	if (!head)
		head = sysfd;
	else {
		while (head)
			head = head->next;
		head->next = sysfd;
	}
	loop->number_fds++;
}
