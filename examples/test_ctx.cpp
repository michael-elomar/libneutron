#include <neutron.hpp>
#include <signal.h>
#include <unistd.h>

class TestHandler : public neutron::Context::Handler {
public:
	TestHandler(bool isServer)
		: mIsServer(isServer), mCtx(new neutron::Context(this))
	{
	}

	~TestHandler()
	{
		delete mCtx;
	}

	inline virtual void onFdCreated(neutron::Context *ctx, int fd) override
	{
		LOGI("FD CREATED");
	}

	inline virtual void onConnected(neutron::Context *ctx,
					neutron::Connection *conn) override
	{
		LOGI("CONNECTED");
	}
	inline virtual void onDisconnected(neutron::Context *ctx,
					   neutron::Connection *conn) override
	{
		LOGI("DISCONNECTED");
	}

	virtual int start(struct sockaddr_storage *addr, uint32_t addrlen) = 0;
	virtual int stop() = 0;

	inline neutron::Loop *getLoop()
	{
		return mCtx->getLoop();
	}

private:
	bool mIsServer;

protected:
	neutron::Context *mCtx;
};

class ServerHandler : public TestHandler {
public:
	ServerHandler() : TestHandler(true) {}

	int start(struct sockaddr_storage *addr, uint32_t addrlen) override
	{
		return mCtx->listen(addr, addrlen);
	}

	int stop() override
	{
		return 0;
	}

	void recvData(neutron::Context *ctx,
		      neutron::Connection *conn,
		      const std::vector<uint8_t> &buf) override
	{
		LOGI("message received");
	};
};

class ClientHandler : public TestHandler {
public:
	ClientHandler() : TestHandler(false) {}

	int start(struct sockaddr_storage *addr, uint32_t addrlen) override
	{
		mCtx->connect(addr, addrlen);
		sendData((uint8_t *)"Hello World", strlen("Hello World"));
		return 0;
	}

	int stop() override
	{
		return 0;
	}

	void recvData(neutron::Context *ctx,
		      neutron::Connection *conn,
		      const std::vector<uint8_t> &buf) override
	{
		LOGI("message received");
	}

	int sendData(uint8_t *buf, uint32_t buflen)
	{
		return mCtx->send(buf, buflen);
	}
};

class App {
public:
	App(bool isServer)
	{
		App::sInstance = this;

		if (isServer)
			mHandler = new ServerHandler();
		else
			mHandler = new ClientHandler();
	}
	~App() {}

	static void sigHandler(int signum)
	{
		sInstance->mRunning = false;
		sInstance->mHandler->getLoop()->wakeup();
	}

	int run(struct sockaddr_storage *addr, uint32_t addrlen)
	{
		mRunning = true;

		signal(SIGINT, &App::sigHandler);
		signal(SIGTERM, &App::sigHandler);

		mHandler->start(addr, addrlen);

		while (mRunning)
			mHandler->getLoop()->spin();

		mHandler->stop();

		return 0;
	}

private:
	static App *sInstance;
	TestHandler *mHandler;
	bool mRunning;
};

App *App::sInstance;

static void usage(const char *progname)
{
	fprintf(stderr, "%s -s <addr>\n", progname);
	fprintf(stderr, "    start server\n");
	fprintf(stderr, "%s -c <addr>\n", progname);
	fprintf(stderr, "    start client\n");
	fprintf(stderr, "<addr> format:\n");
	fprintf(stderr, "  inet:<addr>:<port>\n");
	fprintf(stderr, "  inet6:<addr>:<port>\n");
	fprintf(stderr, "  unix:<path>\n");
	fprintf(stderr, "  unix:@<name>\n");
}

int main(int argc, char **argv)
{
	/* Check arguments */
	if (argc != 3
	    || (strcmp(argv[1], "-s") != 0 && strcmp(argv[1], "-c") != 0)) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	struct sockaddr_storage addr_storage;
	uint32_t addrlen = 0;

	/* Client/Server mode */
	bool isServer = (strcmp(argv[1], "-s") == 0);

	App app(isServer);

	/* Parse address */
	memset(&addr_storage, 0, sizeof(addr_storage));
	addrlen = sizeof(addr_storage);
	if (neutron_ctx_parse_address(argv[2], &addr_storage, &addrlen) < 0) {
		LOGE("Failed to parse address : %s", argv[2]);
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	app.run(&addr_storage, addrlen);
}
