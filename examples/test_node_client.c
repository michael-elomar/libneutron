#include <neutron.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>

int running = 1;
struct neutron_loop *loop;
struct neutron_node *node, *node2;

void sig_handler(int signum)
{
	if (loop)
		neutron_loop_wakeup(loop);
	running = 0;
}

void callback(int conn_fd, void *buf, uint32_t buflen)
{
	LOGD("Inside %s, fd: %d, %s", __func__, conn_fd, (char *)buf);
	send(conn_fd, buf, buflen, 0);
}

int main(int argc, char *argv[])
{
	/* exit on SIGINT & SIGTERM */
	signal(SIGINT, &sig_handler);
	signal(SIGTERM, &sig_handler);

	/* ignore SIGPIPE */
	signal(SIGPIPE, SIG_IGN);
	loop = neutron_loop_create();
	node = neutron_node_create_with_loop(loop, "inet:127.0.0.1:8080", NULL);

	neutron_node_connect(node);
	neutron_node_send(node, (uint8_t *)"test_node_client", 18);

	neutron_node_destroy(node);
	neutron_loop_destroy(loop);

	return 0;
}
