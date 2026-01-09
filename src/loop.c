#include <loop.h>
#include <priv.h>

static inline uint32_t neutron_events_to_epoll(uint32_t neutron_event)
{
	uint32_t epoll_flag = 0;
	if (neutron_event & NEUTRON_FD_EVENT_IN)
		epoll_flag |= EPOLLIN;
	if (neutron_event & NEUTRON_FD_EVENT_ERROR)
		epoll_flag |= EPOLLERR;
	if (neutron_event & NEUTRON_FD_EVENT_HUP)
		epoll_flag |= EPOLLHUP;
	if (neutron_event & NEUTRON_FD_EVENT_OUT)
		epoll_flag |= EPOLLOUT;
	if (neutron_event & NEUTRON_FD_EVENT_PRI)
		epoll_flag |= EPOLLPRI;

	return epoll_flag;
}

static inline uint32_t neutron_events_from_epoll(uint32_t epoll_event)
{
	uint32_t neutron_flag = 0;
	if (epoll_event & EPOLLIN)
		neutron_flag |= NEUTRON_FD_EVENT_IN;
	if (epoll_event & EPOLLOUT)
		neutron_flag |= NEUTRON_FD_EVENT_OUT;
	if (epoll_event & EPOLLERR)
		neutron_flag |= NEUTRON_FD_EVENT_ERROR;
	if (epoll_event & EPOLLHUP)
		neutron_flag |= NEUTRON_FD_EVENT_HUP;
	if (epoll_event & EPOLLPRI)
		neutron_flag |= NEUTRON_FD_EVENT_PRI;

	return neutron_flag;
}

static inline void neutron_loop_register_fd(struct neutron_loop *loop,
					    struct neutron_fd *fd);

struct neutron_loop *neutron_loop_create()
{
	struct neutron_loop *loop = calloc(1, sizeof(struct neutron_loop));
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

int neutron_loop_add(struct neutron_loop *loop,
		     int fd,
		     neutron_fd_event_cb cb,
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
	event->events = neutron_events_to_epoll(events);

	ret = epoll_ctl(loop->efd, EPOLL_CTL_ADD, fd, event);
	if (ret < 0) {
		LOG_ERRNO("cannot add fd to loop");
		return errno;
	}
	free(event);

	struct neutron_fd *nfd = calloc(1, sizeof(struct neutron_fd));
	if (!nfd) {
		LOG_ERRNO("cannot allocate memory for syskit_fd instance");
		free(nfd);
		return errno;
	}
	nfd->fd = fd;
	nfd->events = events;
	nfd->userdata = userdata;
	nfd->cb = cb;

	neutron_loop_register_fd(loop, nfd);

	return 0;
}

int neutron_loop_remove(struct neutron_loop *loop, int fd)
{
	struct neutron_fd *head = loop->nfd;
	struct neutron_fd *to_remove = NULL;

	/* in case the fd to remove is the head of the list */
	if (head->fd == fd) {
		loop->nfd = loop->nfd->next;
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

struct neutron_fd *neutron_loop_find_fd(struct neutron_loop *loop, int fd)
{
	struct neutron_fd *head = loop->nfd;
	while (head && head->fd != fd)
		head = head->next;
	return head;
}

int neutron_loop_spin(struct neutron_loop *loop)
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
		uint32_t revents = neutron_events_from_epoll(events[i].events);
		if (revents == 0)
			continue;

		if (events[i].data.fd == loop->wakeup_fd) {
			uint64_t value;
			read(loop->wakeup_fd, &value, sizeof(uint64_t));
			continue;
		}

		struct neutron_fd *nfd =
			neutron_loop_find_fd(loop, events[i].data.fd);

		if (nfd != NULL && nfd->cb != NULL)
			(*nfd->cb)(nfd->fd, nfd->events, nfd->userdata);
	}
	return 0;
}

void neutron_loop_destroy(struct neutron_loop *loop)
{
	struct neutron_fd *head = loop->nfd;
	struct neutron_fd *aux;
	while (head) {
		aux = head;
		head = head->next;
		free(aux);
	}
	free(loop);
	loop = NULL;
}

void neutron_loop_wakeup(struct neutron_loop *loop)
{
	uint64_t value = LOOP_WAKEUP_MAGIC;
	int ret = write(loop->wakeup_fd, &value, sizeof(uint64_t));
	if (ret < 0) {
		LOG_ERRNO("wakeup write call failed");
	}
}

void neutron_loop_display_registered_fds(struct neutron_loop *loop)
{
	struct neutron_fd *head = loop->nfd;
	while (head) {
		LOGI("fd: %ld", head->fd);
		head = head->next;
	}
}

static inline void neutron_loop_register_fd(struct neutron_loop *loop,
					    struct neutron_fd *fd)
{
	if (loop->number_fds == 0)
		loop->nfd = fd;
	else {
		struct neutron_fd *head = loop->nfd;
		while (head->next)
			head = head->next;
		head->next = fd;
	}
	loop->number_fds++;
}
