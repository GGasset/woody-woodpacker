
#include "random.h"

void fill_randomly(void *buff, size_t size)
{
	int urand_fd = open("/dev/urandom", O_RDONLY);
	if (urand_fd < 0) {dprintf(3, "Unable to open /dev/urandom\n"); return;}

	read(urand_fd, buff, size);
	close(urand_fd);
}
