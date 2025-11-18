#include <loop.h>
#include <priv.h>

static inline uint32_t convert_syskit_to_epoll_event(uint32_t event_flag)
{
	uint32_t epoll_flag = 0;
	if (event_flag | SYSKIT_FD_EVENT_IN)
		epoll_flag |= EPOLLIN;
	else if (event_flag | SYSKIT_FD_EVENT_ERROR)
		epoll_flag |= EPOLLERR;
	else if (event_flag | SYSKIT_FD_EVENT_HUP)
		epoll_flag |= EPOLLHUP;
	else if (event_flag | SYSKIT_FD_EVENT_OUT)
		epoll_flag |= EPOLLOUT;
	else if (event_flag | SYSKIT_FD_EVENT_PRI)
		epoll_flag |= EPOLLPRI;
	else if (event_flag)
		LOGW("event_flag has unsupported flag value: %ud", event_flag);

	return epoll_flag;
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

	return loop;

error:
	free(loop);
	return NULL;
}

int syskit_loop_add(struct syskit_loop *loop,
		    int fd,
		    syskit_fd_event_cb cb,
		    uint32_t events)
{
	int ret = 0;

	struct epoll_event *event = calloc(1, sizeof(struct epoll_event));
	if (!event) {
		LOG_ERRNO("cannot calloc memory for epoll_event");
		free(event);
		return errno;
	}
	event->data.fd = fd;
	event->events = convert_syskit_to_epoll_event(events);

	ret = epoll_ctl(loop->efd, EPOLL_CTL_ADD, fd, event);
	if (ret < 0) {
		LOG_ERRNO("cannot add fd to loop");
		return errno;
	}

	struct syskit_fd *sysfd = calloc(1, sizeof(struct syskit_fd));
	if (!sysfd) {
		LOG_ERRNO("cannot allocate memory for syskit_fd instance");
		free(sysfd);
		return errno;
	}
	sysfd->fd = fd;
	sysfd->events = events;
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

int syskit_loop_spin(struct syskit_loop *loop)
{
	struct epoll_event event = {};
	int nb_events = 0;

	nb_events = epoll_wait(loop->efd, &event, 1, -1);
	if (nb_events < 0) {
		LOG_ERRNO("epoll_wait");
		return errno;
	}

	/* match the received event to the registered fds */
	struct syskit_fd *matched_fd = loop->sfd;
	while (matched_fd && matched_fd->fd != event.data.fd)
		matched_fd = matched_fd->next;

	if (!matched_fd) {
		LOGE("event received for an unregistered fd=%d", event.data.fd);
		return EINVAL;
	}

	if (matched_fd->cb) {
		matched_fd->cb(matched_fd->fd, matched_fd->events, NULL);
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
