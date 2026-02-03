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
	class EventHandler {
	public:
		inline EventHandler() {}
		inline virtual ~EventHandler() {}
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
	Context(EventHandler *eventHandler, Loop *loop = nullptr)
	{
		if (loop == nullptr) {
			mCtx = neutron_ctx_create(&Context::eventCallback,
						  this);
			mIsLoopExternal = false;
			mEventHandler = eventHandler;
			mLoop = new Loop(neutron_ctx_get_loop(mCtx));
		} else {
			mLoop = loop;
			mCtx = neutron_ctx_create_with_loop(
				&Context::eventCallback, loop->getLoop(), this);
			mIsLoopExternal = true;
			mEventHandler = eventHandler;
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

	int disonnect()
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
			ctx->mEventHandler->onConnected(ctx, conn);
			break;
		case NEUTRON_EVENT_DISCONNECTED:
			it = ctx->findConn(_conn);
			if (it != ctx->mConnections.end()) {
				conn = *it;
				ctx->mEventHandler->onDisconnected(ctx, conn);
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
					void *_buf,
					uint32_t _buflen,
					void *_userdata)
	{
		Context *ctx = reinterpret_cast<Context *>(_userdata);
	}

	inline static void fdCallback(struct neutron_ctx *_ctx,
				      int _fd,
				      void *_userdata)
	{
		Context *ctx = reinterpret_cast<Context *>(_userdata);
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
	EventHandler *mEventHandler;
	ConnectionList mConnections;
	Loop *mLoop;
	bool mIsLoopExternal;
};
} // namespace neutron
