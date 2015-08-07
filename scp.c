#include "utiles.h"

int scp_update(char *device_name, char *packet_dir)
{
	int serial_fd;

	serial_fd = serial_init(device_name);
	if (serial_fd < 0) {
		print("%s - serial_init failed", __func__);
		return serial_fd;
	}
	return 0;
}
