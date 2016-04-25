#include <stdio.h>
#include <string.h>

#include "utiles.h"

static void usage(char *app_name)
{
	/* args index:	0	1		2		3		4		5 */
	print("Syntax: %s <serialport> <reset gpio num> <product app dir> [secure app dir] [crk dir]", app_name);
	print("");
	print("\t<serialport> = serial port device (e.g. /dev/ttyS0 (linux) or COM1 (windows))");
	print("\t<reset gpio num> = which gpio can used to reset secure processor");
	print("\t<product app dir> = directory path that contains the SCP packet list, absolute path");
	print("\t[secure app dir] = directory path that contains the SCP packet list, absolute path, optional");
	print("\t[crk dir] = directory path of crk, optional");
}

int main(int argc, void *argv[])
{
	char *device_name, *secure_dir, *product_dir, *gpio_num, *key_dir;
	int ret, burn_key = 0, burn_secure = 0;

	switch (argc) {
	case 6:
		key_dir = argv[5];
		burn_key = 1;
	case 5:
		secure_dir = argv[4];
		burn_secure = 1;
	case 4:
		device_name = argv[1];
		gpio_num = argv[2];
		product_dir = argv[3];
		break;
	default:
		usage(argv[0]);
		ret = -1;
		goto end;
	}

	/* burn CRK */
	if (burn_key) {
		ret = reset_secure_processor(gpio_num);
		if (ret) {
			print("%s - reset secure processor failed", __func__);
			ret = -1;
			goto end;
		}

		/* burn crk */
		ret = scp_send(device_name, key_dir);
		if (ret) {
			print("%s: %s - scp send failed(%s)", argv[0], __func__, key_dir);
			ret = -1;
			goto end;
		}
		print("%s - burn CRK successfully", __func__);
	}

	/* burn secure application */
	if (burn_secure) {
		ret = reset_secure_processor(gpio_num);
		if (ret) {
			print("%s - reset secure processor failed", __func__);
			ret = -1;
			goto end;
		}

		ret = scp_send(device_name, secure_dir);
		if (ret) {
			print("%s: %s - scp send failed(%s)", argv[0], __func__, secure_dir);
			ret = -1;
			goto end;
		}
		print("%s - secure application update successfully", __func__);

		sleep(3);
	}

	/* burn product application */
	ret = reset_secure_processor(gpio_num);
	if (ret) {
		print("%s - reset secure processor failed", __func__);
		ret = -1;
		goto end;
	}

	ret = scp_send(device_name, product_dir);
	if (ret) {
		print("%s: %s - scp send failed", argv[0], __func__, product_dir);
		ret = -1;
		goto end;
	}
	print("%s - product application update successfully", __func__);
	ret = shut_down_processor(gpio_num);
	if (ret)
		print("Failed to shut down secure processor. Please reburn later.\n");
	else
		print("Burn successfully. Please shut dowm.\n");

end:
	show_notify(device_name, ret);

	return 0;
}
