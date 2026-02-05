#include "timer.h"
#include <neutron.h>
#include <string>
#include <vector>
/* Loop wrapping in C++ */

namespace neutron {

class Address;
class Loop;
class Context;
class Connection;
class Event;
class Timer;

class Address {
public:
	Address() : mAddressLen(0), mValid(false) {}
	Address(std::string str)
		: mValid(neutron_ctx_parse_address(str.c_str(),
						   &mSS,
						   &mAddressLen)
			 == 0)
	{
	}

	bool isValid() const
	{
		return mValid;
	}

	socklen_t len() const
	{
		return mAddressLen;
	}

	struct sockaddr_storage *addr()
	{
		return &mSS;
	}

private:
	struct sockaddr_storage mSS;
	socklen_t mAddressLen;
	const bool mValid;
};

class Loop {
public:
	class Handler {
	public:
		inline Handler() {}
		inline virtual ~Handler() {}
		virtual void processEvent(int fd, uint32_t revents) = 0;
	};

public:
	inline Loop()
	{
		mLoop = neutron_loop_create();
	}

	inline Loop(struct neutron_loop *loop)
	{
		mLoop = loop;
	}

	inline ~Loop()
	{
		neutron_loop_destroy(mLoop);
	}

	inline int add(int fd, uint32_t events, Handler *handler)
	{
		return neutron_loop_add(
			mLoop, fd, &fdEventCallback, events, handler);
	}

	inline struct neutron_fd *findFd(int fd)
	{
		return neutron_loop_find_fd(mLoop, fd);
	}

	inline int remove(int fd)
	{
		return neutron_loop_remove(mLoop, fd);
	}

	inline int spin()
	{
		return neutron_loop_spin(mLoop);
	}

	inline void wakeup()
	{
		neutron_loop_wakeup(mLoop);
	}

	inline operator struct neutron_loop *()
	{
		return mLoop;
	}

	inline struct neutron_loop *getLoop()
	{
		return mLoop;
	}

private:
	inline static void fdEventCallback(int _fd,
					   uint32_t _revents,
					   void *_userdata)
	{
		Handler *handler = reinterpret_cast<Handler *>(_userdata);
		handler->processEvent(_fd, _revents);
	}

private:
	struct neutron_loop *mLoop;
};

class Connection {
public:
	Connection(struct neutron_conn *_conn)
	{
		mConn = _conn;
	}

private:
	struct neutron_conn *mConn;
	friend class Context;
};

typedef std::vector<Connection *> ConnectionList;

class Context {
public:
	class Handler {
	public:
		inline Handler() {}
		inline virtual ~Handler() {}

		inline virtual void onFdCreated(Context *ctx, int fd) = 0;

		inline virtual void onConnected(Context *ctx,
						Connection *conn) = 0;
		inline virtual void onDisconnected(Context *ctx,
						   Connection *conn) = 0;
		inline virtual void recvData(
			Context *ctx,
			Connection *conn,
			const std::vector<uint8_t> &buf) = 0;
	};

public:
	Context(Handler *handler, Loop *loop = nullptr)
	{
		if (loop == nullptr) {
			mCtx = neutron_ctx_create(&Context::eventCallback,
						  this);
			mIsLoopExternal = false;
			mHandler = handler;
			mLoop = new Loop(neutron_ctx_get_loop(mCtx));
		} else {
			mLoop = loop;
			mCtx = neutron_ctx_create_with_loop(
				&Context::eventCallback, loop->getLoop(), this);
			mIsLoopExternal = true;
			mHandler = handler;
		}
		setFdCallback();
		setDataCallback();
		setEventCallback();
	}

	~Context()
	{
		neutron_ctx_destroy(mCtx);
		if (mLoop && !mIsLoopExternal)
			delete mLoop;
	}

	int parseAddress(std::string str,
			 struct sockaddr_storage *addr,
			 socklen_t *addrlen)
	{
		return neutron_ctx_parse_address(str.c_str(), addr, addrlen);
	}

	int setDataCallback()
	{
		return neutron_ctx_set_socket_data_cb(mCtx,
						      &Context::dataCallback);
	}

	int setFdCallback()
	{
		return neutron_ctx_set_socket_fd_cb(mCtx, &Context::fdCallback);
	}

	int setEventCallback()
	{
		return neutron_ctx_set_socket_event_cb(mCtx,
						       &Context::eventCallback);
	}

	Loop *getLoop()
	{
		return mLoop;
	}

	int listen(struct sockaddr_storage *addr, ssize_t addrlen)
	{
		return neutron_ctx_listen(mCtx, addr, addrlen);
	}

	int listen(Address &address)
	{
		return neutron_ctx_listen(mCtx, address.addr(), address.len());
	}

	int connect(struct sockaddr_storage *addr, ssize_t addrlen)
	{
		return neutron_ctx_connect(mCtx, addr, addrlen);
	}

	int connect(Address &address)
	{
		return neutron_ctx_connect(mCtx, address.addr(), address.len());
	}

	int bind(struct sockaddr_storage *addr, ssize_t addrlen)
	{
		return neutron_ctx_bind(mCtx, addr, addrlen);
	}

	int bind(Address &address)
	{
		return neutron_ctx_bind(mCtx, address.addr(), address.len());
	}

	int broadcast()
	{
		return neutron_ctx_broadcast(mCtx);
	}

	int disconnect()
	{
		return neutron_ctx_disconnect(mCtx);
	}

	int send(uint8_t *buf, uint32_t buflen)
	{
		return neutron_ctx_send(mCtx, buf, buflen);
	}

	int sendTo(struct sockaddr_storage *addr,
		   ssize_t addrlen,
		   uint8_t *buf,
		   uint32_t buflen)
	{
		return neutron_ctx_send_to(mCtx, addr, addrlen, buf, buflen);
	}

	int sentTo(Address &address, uint8_t *buf, uint32_t buflen)
	{
		return neutron_ctx_send_to(
			mCtx, address.addr(), address.len(), buf, buflen);
	}

private:
	inline static void eventCallback(struct neutron_ctx *_ctx,
					 enum neutron_event _event,
					 struct neutron_conn *_conn,
					 void *_userdata)
	{
		Context *ctx = reinterpret_cast<Context *>(_userdata);
		Connection *conn = nullptr;
		ConnectionList::iterator it;

		switch (_event) {
		case NEUTRON_EVENT_CONNECTED:
			conn = new Connection(_conn);
			ctx->mConnections.push_back(conn);
			ctx->mHandler->onConnected(ctx, conn);
			break;
		case NEUTRON_EVENT_DISCONNECTED:
			it = ctx->findConn(_conn);
			if (it != ctx->mConnections.end()) {
				conn = *it;
				ctx->mHandler->onDisconnected(ctx, conn);
				ctx->mConnections.erase(it);
				delete conn;
			}
			break;
		case NEUTRON_EVENT_DATA:
			break;
		default:
			break;
		}
	}

	inline static void dataCallback(struct neutron_ctx *_ctx,
					struct neutron_conn *_conn,
					uint8_t *_buf,
					uint32_t _buflen,
					void *_userdata)
	{
		Context *ctx = reinterpret_cast<Context *>(_userdata);
		Connection *conn = nullptr;
		ConnectionList::iterator it;
		bool created = false;

		std::vector<uint8_t> buf(&_buf[0], &_buf[_buflen]);

		it = ctx->findConn(_conn);
		if (it != ctx->mConnections.end())
			conn = *it;
		else {
			conn = new Connection(_conn);
			created = true;
		}

		ctx->mHandler->recvData(ctx, conn, buf);

		if (created)
			delete conn;
	}

	inline static void fdCallback(struct neutron_ctx *_ctx,
				      int _fd,
				      void *_userdata)
	{
		Context *ctx = reinterpret_cast<Context *>(_userdata);
		ctx->mHandler->onFdCreated(ctx, _fd);
	}

	inline ConnectionList::iterator findConn(struct neutron_conn *_conn)
	{
		ConnectionList::iterator it;
		for (it = mConnections.begin(); it != mConnections.end();
		     it++) {
			if ((*it)->mConn == _conn)
				return it;
		}
		return mConnections.end();
	}

private:
	struct neutron_ctx *mCtx;
	Handler *mHandler;
	ConnectionList mConnections;
	Loop *mLoop;
	bool mIsLoopExternal;
};

class Timer {
public:
	class Handler {
	public:
		inline Handler() {}
		inline virtual ~Handler() {}
		inline virtual void processTimer() = 0;
	};

public:
	Timer(Loop *loop, Handler *handler) : mLoop(loop), mHandler(handler)
	{
		mTimer = neutron_timer_create(
			mLoop->getLoop(), &Timer::timerCallback, this);
	}
	~Timer()
	{
		neutron_timer_destroy(mTimer);
	}

	int set(uint32_t delay, uint32_t period = 0)
	{
		if (period == 0)
			return neutron_timer_set(mTimer, delay);
		else
			return neutron_timer_set_periodic(
				mTimer, delay, period);
	}

	int clear()
	{
		return neutron_timer_clear(mTimer);
	}

private:
	static void timerCallback(struct neutron_timer *_timer, void *_userdata)
	{
		Timer *timer = reinterpret_cast<Timer *>(_userdata);
		timer->mHandler->processTimer();
	}

private:
	struct neutron_timer *mTimer;
	Loop *mLoop;
	Handler *mHandler;
};

class Event {
public:
	class Handler {
	public:
		inline Handler() {}
		inline virtual ~Handler() {}
		inline virtual void processEvent() = 0;
	};

public:
	Event(int flags = 0)
	{
		mEvt = neutron_evt_create(flags, &Event::evtCallback);
	}

	~Event()
	{
		neutron_evt_destroy(mEvt);
	}

	int attachLoop(Loop *loop, Handler *handler)
	{
		mHandler = handler;
		return neutron_evt_attach(mEvt, loop->getLoop());
	}

	int detachLoop(Loop *loop)
	{
		return neutron_evt_detach(mEvt, loop->getLoop());
	}

	int trigger()
	{
		return neutron_evt_trigger(mEvt);
	}

	int clear()
	{
		return neutron_evt_clear(mEvt);
	}

private:
	static void evtCallback(struct neutron_evt *_evt, void *_userdata)
	{
		Event *event = reinterpret_cast<Event *>(_userdata);
		event->mHandler->processEvent();
	}

private:
	struct neutron_evt *mEvt;
	Handler *mHandler;
};

} // namespace neutron
