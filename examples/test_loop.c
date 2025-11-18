#include <syskit.h>

int main(int argc, char *argv[])
{
	struct syskit_loop *loop = syskit_loop_create();
	printf("loop address : %d\n", loop);
	return 0;
}
