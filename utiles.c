#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "utiles.h"

void print(const char *fmt, ...)
{
	va_list ap;
	const char *p = fmt;
	char cval, *sval;
	int ival;

	if (!fmt)
		return;
	va_start(ap, fmt);
	while (*p != '\0') {
		if (*p != '%') {
			putchar(*p++);
			continue;
		}
		switch (*++p) {
		case 'd':
			ival = va_arg(ap, int);
			printf("%d", ival);
			break;
		case 'c':
			cval = (char)va_arg(ap, int);
			printf("%c", cval);
			break;
		case 's':
			sval = va_arg(ap, char *);
			printf("%s", sval);
			break;
		case 'x':
			ival = va_arg(ap, int);
			printf("%x", ival);
			break;
		}
		p ++;
	}
	va_end(ap);
	putchar('\n');

	return;
}

void display_progress_bar(double progress)
{
	double size = 100.0 / PROGRESS_BAR_SIZE;
	int i;
	double count;
	double percent;

	percent = progress * 100.0;
	if (percent < 0.0 || percent > 100.0) {
		printf("LC: %s - invalide argument, percent = %f\n", __func__, percent);
		return;
	}
	for (count = 0.0; count < PROGRESS_BAR_SIZE; count ++) {
		if (percent <= count * size)
			break;
	}
	printf("\r["); /* move the cursor to the beginning of the line */
	for (i = 0; i < PROGRESS_BAR_SIZE; i ++) {
		if (i <  count)
			printf("%s", PROGRESS_BAR_CONTENT);
		else
			printf("%s", " ");
	}
	printf("] %.2f%%", count * size);
	fflush(stdout);
	/* if (count == PROGRESS_BAR_SIZE) */
	/* 	putchar('\n'); */
}

/* return */
/* 1: big endian */
/* 0: small endian */
/* -1: error */
int is_big_endian()
{
	unsigned int a = 0xffffff00;
	unsigned char *b = (char *)&a;

	if (*b == 0xff)
		return 1;
	else if (*b == 0)
		return 0;
	else
		return -1;
}

void small_endian_2_big_endian(void *data)
{
	unsigned char *index = data;
	unsigned char tmp[4];
	int i;

	memcpy(tmp, data, 4);
	for (i = 0; i < 4; i ++) {
		*(index + i) = tmp[4 - i - 1];
	}
}

void show_notify(char *device_name, int status)
{
	int fd;
	char on = 0xff;
	char off = 0x0;
	int off_long_time[5000] = {0};

	fd = open(device_name, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd < 0) {
		print("%s - open device failed, errno = %d", __func__, errno);
		return;
	}
	if (status) {
		/* fail */
		while(1) {
			write(fd, &off, 1);
		}
	} else {
		/* success */
		while(1) {
			write(fd, off_long_time, sizeof(off_long_time));
			sleep(2);
		}
	}
	close(fd);
}

int shut_down_processor(char *gpio_num)
{
	char gpio_path[GPIO_PATH_MAX_SIZE] = {0};
	int fd, ret;

	snprintf(gpio_path, sizeof(gpio_path), "%s/gpio%s/value\0", GPIO_PREFIX_PATH, gpio_num);
	fd = open(gpio_path, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd < 0) {
		print("%s - open device failed, errno = %d", __func__, errno);
		return -1;
	}
	ret = write(fd, "0", 1);
	if (ret != 1) {
		print("%s - failed to pull down gpio, ret = %d, errno = %d\n", __func__, ret, errno);
		close(fd);
		return -1;
	}
	close(fd);

	return 0;
}

int reset_secure_processor(char *gpio_num)
{
	char gpio_path[GPIO_PATH_MAX_SIZE] = {0};
	int fd, ret;

	snprintf(gpio_path, sizeof(gpio_path), "%s/gpio%s/value\0", GPIO_PREFIX_PATH, gpio_num);
	fd = open(gpio_path, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd < 0) {
		print("%s - open device failed, errno = %d", __func__, errno);
		return -1;
	}
	ret = write(fd, "0", 1);
	if (ret != 1) {
		print("%s - failed to pull down gpio, ret = %d, errno = %d\n", __func__, ret, errno);
		close(fd);
		return -1;
	}
	sleep(1);
	ret = write(fd, "1", 1);
	if (ret != 1) {
		print("%s - failed to pull up gpio, ret = %d, errno = %d\n", __func__, ret, errno);
		close(fd);
		return -1;
	}
	close(fd);

	return 0;
}
