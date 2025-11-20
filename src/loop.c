#include <loop.h>
#include <priv.h>

static inline uint32_t syskit_events_to_epoll(uint32_t syskit_event)
{
	uint32_t epoll_flag = 0;
	if (syskit_event & SYSKIT_FD_EVENT_IN)
		epoll_flag |= EPOLLIN;
	if (syskit_event & SYSKIT_FD_EVENT_ERROR)
		epoll_flag |= EPOLLERR;
	if (syskit_event & SYSKIT_FD_EVENT_HUP)
		epoll_flag |= EPOLLHUP;
	if (syskit_event & SYSKIT_FD_EVENT_OUT)
		epoll_flag |= EPOLLOUT;
	if (syskit_event & SYSKIT_FD_EVENT_PRI)
		epoll_flag |= EPOLLPRI;

	return epoll_flag;
}

static inline uint32_t syskit_events_from_epoll(uint32_t epoll_event)
{
	uint32_t syskit_flag = 0;
	if (epoll_event & EPOLLIN)
		syskit_flag |= SYSKIT_FD_EVENT_IN;
	if (epoll_event & EPOLLOUT)
		syskit_flag |= SYSKIT_FD_EVENT_OUT;
	if (epoll_event & EPOLLERR)
		syskit_flag |= SYSKIT_FD_EVENT_ERROR;
	if (epoll_event & EPOLLHUP)
		syskit_flag |= SYSKIT_FD_EVENT_HUP;
	if (epoll_event & EPOLLPRI)
		syskit_flag |= SYSKIT_FD_EVENT_PRI;

	return syskit_flag;
}

static inline void syskit_loop_register_fd(struct syskit_loop *loop,
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

	loop->wakeup_fd = eventfd(0, 0);
	if (loop->wakeup_fd < 0) {
		LOG_ERRNO("cannot create wakeup eventfd");
		goto error;
	}

	struct epoll_event *event = calloc(1, sizeof(struct epoll_event));
	if (!event) {
		LOG_ERRNO("cannot calloc memory for epoll_event");
		free(event);
		goto error;
	}

	event->data.fd = loop->wakeup_fd;
	event->events = EPOLLIN;
	int ret = epoll_ctl(loop->efd, EPOLL_CTL_ADD, loop->wakeup_fd, event);
	if (ret < 0) {
		LOG_ERRNO("epoll_ctl - failed to add wakeup fd to event loop");
		goto error;
	}
	free(event);
	return loop;

error:
	free(event);
	free(loop);
	return NULL;
}

int syskit_loop_add(struct syskit_loop *loop,
		    int fd,
		    syskit_fd_event_cb cb,
		    uint32_t events,
		    void *userdata)
{
	int ret = 0;

	struct epoll_event *event = calloc(1, sizeof(struct epoll_event));
	if (!event) {
		LOG_ERRNO("cannot calloc memory for epoll_event");
		free(event);
		return errno;
	}
	event->data.fd = fd;
	event->events = syskit_events_to_epoll(events);

	ret = epoll_ctl(loop->efd, EPOLL_CTL_ADD, fd, event);
	if (ret < 0) {
		LOG_ERRNO("cannot add fd to loop");
		return errno;
	}
	free(event);

	struct syskit_fd *sysfd = calloc(1, sizeof(struct syskit_fd));
	if (!sysfd) {
		LOG_ERRNO("cannot allocate memory for syskit_fd instance");
		free(sysfd);
		return errno;
	}
	sysfd->fd = fd;
	sysfd->events = events;
	sysfd->userdata = userdata;
	sysfd->cb = cb;

	syskit_loop_register_fd(loop, sysfd);

	return 0;
}

int syskit_loop_remove(struct syskit_loop *loop, int fd)
{
	struct syskit_fd *head = loop->sfd;
	struct syskit_fd *to_remove = NULL;

	/* in case the fd to remove is the head of the list */
	if (head->fd == fd) {
		loop->sfd = loop->sfd->next;
		to_remove = head;
	} else {
		while (head->next) {
			if (head->next->fd == fd)
				break;
			head = head->next;
		}

		if (!head->next) {
			LOGE("fd=%d is not registered", fd);
			return ENOENT;
		}

		to_remove = head->next;

		head->next = head->next->next;
		loop->number_fds--;
	}

	int ret = epoll_ctl(loop->efd, EPOLL_CTL_DEL, fd, NULL);
	if (ret < 0) {
		LOGE("cannot remove fd=%d from loop", fd);
		LOG_ERRNO("epoll_ctl");
		return errno;
	}

	free(to_remove);
	return 0;
}

struct syskit_fd *syskit_loop_find_fd(struct syskit_loop *loop, int fd)
{
	struct syskit_fd *head = loop->sfd;
	while (head && head->fd != fd)
		head = head->next;
	return head;
}

int syskit_loop_spin(struct syskit_loop *loop)
{
	int ret = 0;
	struct epoll_event events[16];
	eventfd_t value;
	uint32_t nevents = 0;

	memset(events, 0, sizeof(events));
	nevents = sizeof(events) / sizeof(struct epoll_event);

	do {
		ret = epoll_wait(loop->efd, events, nevents, -1);
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		LOG_ERRNO("epoll_wait");
		return errno;
	}

	nevents = ret;
	for (uint32_t i = 0; i < nevents; i++) {
		uint32_t revents = syskit_events_from_epoll(events[i].events);
		if (revents == 0)
			continue;

		if (events[i].data.fd == loop->wakeup_fd) {
			uint64_t value;
			read(loop->wakeup_fd, &value, sizeof(uint64_t));
			continue;
		}

		struct syskit_fd *sfd =
			syskit_loop_find_fd(loop, events[i].data.fd);

		if (sfd != NULL && sfd->cb != NULL)
			(*sfd->cb)(sfd->fd, sfd->events, NULL);
	}
	return 0;
}

void syskit_loop_destroy(struct syskit_loop *loop)
{
	struct syskit_fd *head = loop->sfd;
	struct syskit_fd *aux;
	while (head) {
		aux = head;
		head = head->next;
		free(aux);
	}
	free(loop);
	loop = NULL;
}

void syskit_loop_wakeup(struct syskit_loop *loop)
{
	uint64_t value = LOOP_WAKEUP_MAGIC;
	int ret = write(loop->wakeup_fd, &value, sizeof(uint64_t));
	if (ret < 0) {
		LOG_ERRNO("wakeup write call failed");
	}
}

void syskit_loop_display_registered_fds(struct syskit_loop *loop)
{
	struct syskit_fd *head = loop->sfd;
	while (head) {
		LOGI("fd: %ld", head->fd);
		head = head->next;
	}
}

static inline void syskit_loop_register_fd(struct syskit_loop *loop,
					   struct syskit_fd *sysfd)
{
	if (loop->number_fds == 0)
		loop->sfd = sysfd;
	else {
		struct syskit_fd *head = loop->sfd;
		while (head->next)
			head = head->next;
		head->next = sysfd;
	}
	loop->number_fds++;
}
