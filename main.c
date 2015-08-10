#include <stdio.h>
#include <string.h>

#include "utiles.h"

static void usage(char *app_name)
{
	print("Syntax: %s <serialport> <input_dir>", app_name);
	print("");
	print("\t<serialport> = serial port device (e.g. /dev/ttyS0 (linux) or COM1 (windows))");
	print("\t<input_dir> = directory that contains the SCP packet list, absolute path");
}

int main(int argc, void *argv[])
{
	char *device_name, *packet_dir;
	int ret;

	if (argc == 3) {
		device_name = argv[1];
		packet_dir = argv[2];

	} else {
		usage(argv[0]);
		return -EINVAL;
	}

	ret = scp_burn(device_name, packet_dir);
	if (ret) {
		print("%s: %s - scp_burn failed", argv[0], __func__);
		return -1;
	}
	print("%s - success", __func__);

	return 0;
}
