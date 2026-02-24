#include <neutron.h>
#include <signal.h>

struct main_ctx {
	struct neutron_loop *loop;
	struct neutron_timer *timer;
	int running;
};

struct main_ctx self;

void timer_cb(struct neutron_timer *timer, void *userdata)
{
	LOGI("Inside timer_cb");
}

void sig_handler(int signum)
{
	if (self.loop)
		neutron_loop_wakeup(self.loop);
	self.running = 0;
}

int main(int argc, char **argv)
{
	/* exit on SIGINT & SIGTERM */
	signal(SIGINT, &sig_handler);
	signal(SIGTERM, &sig_handler);

	/* ignore SIGPIPE */
	signal(SIGPIPE, SIG_IGN);

	self.loop = neutron_loop_create();
	self.timer = neutron_timer_create_with_loop(self.loop, timer_cb, NULL);

	neutron_timer_set_periodic(self.timer, 2000, 100);

	self.running = 1;

	while (self.running)
		neutron_loop_spin(self.loop);

	neutron_timer_destroy(self.timer);
	neutron_loop_destroy(self.loop);
}
