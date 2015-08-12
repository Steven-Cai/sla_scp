#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

#include "utiles.h"
#include "serial.h"

static int serial_set_options(int fd)
{
	struct termios options;

	if (tcgetattr(fd, &options)) {
		print("%s - tcgetattr failed, errno = %d", __func__, errno);
		return -1;
	}
	options.c_cflag |= (CLOCAL | CREAD);
	options.c_cflag &= ~CSIZE;
	options.c_cflag &= ~CRTSCTS; // no hardware flow control
	options.c_cflag |= CS8; // data size
	options.c_cflag &= ~CSTOPB; // one stopbit
	options.c_iflag |= IGNPAR; // no parity
	options.c_oflag = 0; // output mode
	options.c_lflag = 0; // disable terminal mode
	if (cfsetospeed(&options, B115200) || cfsetispeed(&options, B115200)) {
		print("%s - set baudrate failed, errno = %d", __func__, errno);
		return -1;
	}

	tcflush(fd, TCOFLUSH);
	tcsetattr(fd, TCSANOW, &options);

	return 0;
}

static int serial_open(const char *device_name)
{
	int fd;

	fd = open(device_name, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd < 0) {
		print("%s - open device failed, errno = %d", __func__, errno);
		return -1;
	}
	// set fd to block
	if (fcntl(fd, F_SETFL, 0) < 0) {
		print("%s - set fd to block failed, errno = %d", __func__, errno);
		return -1;
	}
	// It will test whether the fd is a tty device, in order to ensure that the device is opened correctly
	if (!isatty(STDIN_FILENO)) {
		print("%s - Standard input isn't a terminal device", __func__);
		return -1;
	}

	return fd;
}

int serial_send(int fd, void *data, int data_len)
{
	int len = 0;

	len = write(fd, data, data_len);
	if (len == data_len) {
		print("%s - send %d bytes", __func__, len);
		return len;
	} else {
		print("%s - send data error, len = %d, errno = %d", __func__, len, errno);
		tcflush(fd, TCOFLUSH);
		return -1;
	}
	if (fsync(fd)) {
		print("%s - fsync failed, errno = %d", __func__, errno);
		return -1;
	}
}

/* return: */
/* success: data_len */
/* error: -1 */
/* timeout: -2 */
int serial_receive(int fd, char *data, int datalen)
{
	int len=0, ret = 0;
	fd_set read_fds;
	struct timeval tv_timeout;

	FD_ZERO(&read_fds);
	FD_SET(fd, &read_fds);
	tv_timeout.tv_sec = SERIAL_TIME_OUT;
	tv_timeout.tv_usec = 0;

	ret = select(fd + 1, &read_fds, NULL, NULL, &tv_timeout);
	if (ret < 0) {
		print("%s - select failed. errno = %d", __func__, errno);
		return -1;
	} else if (ret == 0) {
		print("%s - select timeout", __func__);
		return -2;
	}

	if (FD_ISSET(fd, &read_fds)) {
		len = read(fd, data, datalen);
		if (len > 0) {
			print("%s - len = %d, data = %s", __func__, len, data);
			return len;
		} else {
			print("%s - read failed, errno = %d", __func__, errno);
			return -1;
		}
	} else {
		perror("select");
		return -1;
	}

	return 0;
}

int serial_init(const char *device_name)
{
	int fd, ret;
	struct termios *option;

	fd = serial_open(device_name);
	if (fd < 0) {
		print("%s - serial_open failed", __func__);
		return -1;
	}
	ret = serial_set_options(fd);
	if (ret) {
		print("%s - serial_set_options failed", __func__);
		return -1;
	}
	print("%s - success", __func__);

	return fd;
}
