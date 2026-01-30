#include <timer.h>
#include <priv.h>

static void timer_fd_cb(int fd, uint32_t revents, void *userdata)
{
	struct neutron_timer *timer = userdata;
	uint64_t val;
	int ret;

	/* TODO: read from timerfd and check validity of value */
	do {
		ret = read(timer->tfd, &val, sizeof(val));
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		if (errno == EAGAIN)
			LOGW("Timer is empty");
		else
			LOGE("Timer error");
		return;
	}

	if (!ret) {
		LOGE("Invalid read on timer fd: EOF");
		return;
	}

	if (sizeof(val) != ret) {
		LOGE("Partial or invalid read on timer fd");
		return;
	}

	if (!val) {
		LOGE("Callback without timer expiration: this should not happen");
	}

	if (val > 1)
		LOGW("Timer expirations missed: %lu", val - 1);

	if (timer->cb)
		(*timer->cb)(timer, timer->userdata);
}

struct neutron_timer *neutron_timer_create(struct neutron_loop *loop,
					   neutron_timer_cb cb,
					   void *userdata)
{
	if (!loop) {
		LOGE("Failure: loop is null");
		return NULL;
	}

	int ret = 0;
	struct neutron_timer *timer = calloc(1, sizeof(struct neutron_timer));
	if (!timer) {
		LOG_ERRNO(
			"Failure: cannot create allocate memory for neutron timer");
		return NULL;
	}

	timer->loop = loop;
	timer->cb = cb;
	timer->userdata = userdata;
	timer->tfd = -1;

	timer->tfd =
		timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);

	if (timer->tfd < 0) {
		LOG_ERRNO("Failed to create timer fd");
	}

	ret = neutron_loop_add(timer->loop,
			       timer->tfd,
			       timer_fd_cb,
			       NEUTRON_FD_EVENT_IN,
			       (void *)timer);
	if (ret < 0)
		goto clean;

	return timer;

clean:
	if (timer)
		neutron_timer_destroy(timer);
	return NULL;
}

void neutron_timer_destroy(struct neutron_timer *timer)
{
	if (timer) {
		close(timer->tfd);
		timer->tfd = -1;
		free(timer);
		timer = NULL;
	}
}

int neutron_timer_set(struct neutron_timer *timer,
		      uint32_t delay,
		      uint32_t period)
{
	struct itimerspec new_val;
	new_val.it_value.tv_sec = (int)(delay / 1000);
	new_val.it_value.tv_nsec = (delay - new_val.it_value.tv_sec) * 1e+6;

	new_val.it_interval.tv_sec = (int)(period / 1000);
	new_val.it_interval.tv_nsec =
		(delay - new_val.it_interval.tv_sec) * 1e+6;

	int ret = timerfd_settime(timer->tfd, 0, &new_val, NULL);
	if (ret < 0) {
		ret = errno;
		LOG_ERRNO("Failure: timerfd_settime");
	}
	return ret;
}

int neutron_timer_clear(struct neutron_timer *timer)
{
	if (!timer) {
		LOGE("Failure: cannot clear null timer");
		return EINVAL;
	}
	return neutron_timer_set(timer, 0, 0);
}
