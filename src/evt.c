#include <neutron.h>
#include <evt.h>

static void evt_callback(int fd, uint32_t revents, void *userdata)
{
	struct neutron_evt *evt = (struct neutron_evt *)userdata;
	neutron_evt_clear(evt);
	if (revents & NEUTRON_FD_EVENT_IN)
		if (evt->cb)
			(*evt->cb)(evt, evt->userdata);
}

struct neutron_evt *neutron_evt_create(int flags)
{
	struct neutron_evt *evt = calloc(1, sizeof(struct neutron_evt));
	if (!evt) {
		LOG_ERRNO("Failure: cannot allocate memory for neutron evt");
		return NULL;
	}

	evt->fd = eventfd(0, flags);

	if (evt->fd < 0) {
		LOG_ERRNO("Failure: cannot create eventfd");
		goto clean;
	}

clean:
	free(evt);
	evt = NULL;
	return evt;
}

void neutron_evt_destroy(struct neutron_evt *evt)
{
	if (evt) {
		close(evt->fd);
		free(evt);
		evt = NULL;
	}
}

int neutron_evt_trigger(struct neutron_evt *evt)
{
	int ret = 0;
	uint64_t count = NEUTRON_EVT_MAGIC;
	if (!evt) {
		LOGE("Failure: evt is null");
		return EINVAL;
	}

	do {
		ret = write(evt->fd, &count, sizeof(count));
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		LOGE("Failure: cannot write to eventfd");
		ret = errno;

		if (ret == EAGAIN)
			return 0;
		return ret;
	}

	if (ret != sizeof(count))
		return EIO;

	return 0;
}

int neutron_evt_clear(struct neutron_evt *evt)
{
	int ret = 0;
	uint64_t count;

	do {
		ret = read(evt->fd, &count, sizeof(count));
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		LOGE("Failure: cannot read from eventfd");
		ret = errno;

		if (ret == EAGAIN)
			return 0;
		return ret;
	}
	if (ret != sizeof(count) || count != NEUTRON_EVT_MAGIC)
		return EIO;

	return 0;
}

int neutron_evt_attach(struct neutron_evt *evt, struct neutron_loop *loop)
{
	int ret = 0;

	if (!evt) {
		LOGE("Failure: evt is null");
		return EINVAL;
	}

	if (!loop) {
		LOGE("Failure: loop is null");
		return EINVAL;
	}

	evt->loop = loop;

	ret = neutron_loop_add(
		loop, evt->fd, evt_callback, NEUTRON_FD_EVENT_IN, (void *)evt);

	if (ret) {
		LOGE("Failed to attach neutron evt to loop");
		return ret;
	}

	return 0;
}

int neutron_evt_detach(struct neutron_evt *evt, struct neutron_loop *loop)
{
	int ret = 0;

	ret = neutron_loop_remove(loop, evt->fd);
	if (ret) {
		LOGE("Failure: cannot detach eventfd from loop");
		return ret;
	}
	return 0;
}
