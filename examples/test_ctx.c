#include <neutron.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>

int running = 1;
struct neutron_loop *loop;
struct neutron_ctx *ctx;

char PING[] = "PING";
char PONG[] = "PONG";

void sig_handler(int signum)
{
	if (loop)
		neutron_loop_wakeup(loop);
	running = 0;
}

void server_data_cb(struct neutron_ctx *ctx,
		    struct neutron_conn *conn,
		    uint8_t *buf,
		    uint32_t buflen,
		    void *userdata)
{
	LOGD("Server Received: %s", (char *)buf);
	neutron_ctx_send(ctx, (uint8_t *)PONG, strlen(PONG));
}

void client_data_cb(struct neutron_ctx *ctx,
		    struct neutron_conn *conn,
		    uint8_t *buf,
		    uint32_t buflen,
		    void *userdata)
{
	LOGD("Client received: %s", (char *)buf);
	neutron_ctx_send(ctx, (uint8_t *)PING, strlen(PING));
}

void server_event_cb(struct neutron_ctx *ctx,
		     enum neutron_event event,
		     struct neutron_conn *conn,
		     void *userdata)
{
	if (event == NEUTRON_EVENT_CONNECTED) {
		LOGI("CONNECTED EVENT RECEIVED");
	} else if (event == NEUTRON_EVENT_DISCONNECTED) {
		LOGI("DISCONNECTED EVENT RECEIVED");
	} else if (event == NEUTRON_EVENT_DATA) {
		LOGI("DATA EVENT RECEIVED");
	}
}

void client_event_cb(struct neutron_ctx *ctx,
		     enum neutron_event event,
		     struct neutron_conn *conn,
		     void *userdata)
{
	LOGI("Inside %s", __func__);
}

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

int main(int argc, char *argv[])
{
	/* exit on SIGINT & SIGTERM */
	signal(SIGINT, &sig_handler);
	signal(SIGTERM, &sig_handler);

	/* ignore SIGPIPE */
	signal(SIGPIPE, SIG_IGN);

	int is_server;

	/* Check arguments */
	if (argc != 3
	    || (strcmp(argv[1], "-s") != 0 && strcmp(argv[1], "-c") != 0)) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Client/Server mode */
	is_server = (strcmp(argv[1], "-s") == 0);

	loop = neutron_loop_create();
	ctx = neutron_ctx_create_with_loop(
		is_server ? server_event_cb : client_event_cb, loop, NULL);

	struct sockaddr_storage addr;
	socklen_t addrlen;
	neutron_ctx_parse_address(argv[2], &addr, &addrlen);

	if (is_server) {
		neutron_ctx_listen(ctx, &addr, addrlen);
		neutron_ctx_set_socket_data_cb(ctx, server_data_cb);
	} else {
		neutron_ctx_connect(ctx, &addr, addrlen);
		neutron_ctx_set_socket_data_cb(ctx, client_data_cb);
		neutron_ctx_send(ctx, (uint8_t *)PING, strlen(PING));
	}

	while (running) {
		neutron_loop_spin(loop);
	}

	neutron_ctx_destroy(ctx);
	neutron_loop_destroy(loop);

	return 0;
}
