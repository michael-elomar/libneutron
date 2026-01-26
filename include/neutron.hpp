#include <neutron.h>
#include <string>
/* Loop wrapping in C++ */

class NeutronLoop {
public:
	NeutronLoop()
	{
		mLoop = neutron_loop_create();
	}

	~NeutronLoop()
	{
		neutron_loop_destroy(mLoop);
	}

	int add(int fd, neutron_fd_event_cb cb, uint32_t events, void *userdata)
	{
		return neutron_loop_add(mLoop, fd, cb, events, userdata);
	}

	struct neutron_fd *findFd(int fd)
	{
		return neutron_loop_find_fd(mLoop, fd);
	}

	int remove(int fd)
	{
		return neutron_loop_remove(mLoop, fd);
	}

	int spin()
	{
		return neutron_loop_spin(mLoop);
	}

	void wakeup()
	{
		neutron_loop_wakeup(mLoop);
	}

	struct neutron_loop *getLoop()
	{
		return mLoop;
	}

private:
	struct neutron_loop *mLoop;
};

class NeutronCtx {
public:
	NeutronCtx(neutron_ctx_event_cb cb, void *userdata)
	{
		mCtx = neutron_ctx_create(cb, userdata);
	}

	NeutronCtx(neutron_ctx_event_cb cb, NeutronLoop *loop, void *userdata)
	{
		mCtx = neutron_ctx_create_with_loop(
			cb, loop->getLoop(), userdata);
	}

	struct sockaddr_storage *parseAddress(std::string address)
	{
		return neutron_ctx_parse_address(address.c_str());
	}

	int setSocketDataCallback(neutron_ctx_data_cb cb)
	{
		return neutron_ctx_set_socket_data_cb(mCtx, cb);
	}

	int setSocketFdCallback(neutron_ctx_fd_cb cb)
	{
		return neutron_ctx_set_socket_fd_cb(mCtx, cb);
	}

	int setSocketEventCallback(neutron_ctx_event_cb cb)
	{
		return neutron_ctx_set_socket_event_cb(mCtx, cb);
	}

	int listen(struct sockaddr *addr, ssize_t addrlen)
	{
		return neutron_ctx_listen(mCtx, addr, addrlen);
	}

	int connect(struct sockaddr *addr, ssize_t addrlen)
	{
		return neutron_ctx_connect(mCtx, addr, addrlen);
	}

	int bind(struct sockaddr *addr, ssize_t addrlen)
	{
		return neutron_ctx_bind(mCtx, addr, addrlen);
	}

	int broadcast()
	{
		return neutron_ctx_broadcast(mCtx);
	}

	int disonnect()
	{
		return neutron_ctx_disconnect(mCtx);
	}

	int send(uint8_t *buf, uint32_t buflen)
	{
		return neutron_ctx_send(mCtx, buf, buflen);
	}

	int sendTo(const struct sockaddr *addr,
		   ssize_t addrlen,
		   uint8_t *buf,
		   uint32_t buflen)
	{
		return neutron_ctx_send_to(mCtx, addr, addrlen, buf, buflen);
	}

	~NeutronCtx()
	{
		neutron_ctx_destroy(mCtx);
	}

private:
	struct neutron_ctx *mCtx;
};
