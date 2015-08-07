#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "utiles.h"

int serial_init(const char *device_name)
{
	int dev_fd;

	dev_fd = open(device_name, O_RDWR);
	if (dev_fd < 0) {
		print("%s - open device failed, errno = %d", __func__, errno);
		return -1;
	}
	return 0;
}
