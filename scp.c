#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "utiles.h"

#define SCP_PACKET_PATH_MAX_SIZE 1024
#define SCP_PACKET_HOST_MAX_SIZE 4096
#define SCP_PACKET_BL_MAX_SIZE 10240

#define SCP_PACKET_LIST_NAME "packet.list"
#define SCP_PACKET_BL_EMULATION_MODE "bl"
#define SCP_PACKET_HOST_MODE "host"
#define SCP_PACKET_SEND_RETRY_TIMES 15

void packet_file_path_create(const char *packet_dir, const char *packet_name, char *packet_path, int max_size)
{
	int dir_len = strlen(packet_dir);
	int name_len = strlen(packet_name);

	if (dir_len + name_len + 1 <= max_size) {
		strncpy(packet_path, packet_dir, dir_len);
		strncpy(packet_path + dir_len, packet_name, name_len + 1); // +1: '\0'
	} else
		packet_path[0] = '\0';
}

FILE *packet_list_open(const char *packet_dir)
{
	FILE *fp = NULL;
	int dir_len = strlen(packet_dir);
	char *packet_list = SCP_PACKET_LIST_NAME;
	int list_len = strlen(packet_list);
	int max_path_size = dir_len + list_len + 1;
	char *packet_path = calloc(1, max_path_size);

	packet_file_path_create(packet_dir, packet_list, packet_path, max_path_size);
	fp = fopen(packet_path, "r");
	if (!fp) {
		print("%s - fopen(%s) failed, errno = %d", __func__, packet_path, errno);
		free(packet_path);
		return fp;
	}
	free(packet_path);

	return fp;
}

void packet_list_close(FILE *fp)
{
	fclose(fp);
}

int packet_list_read(int fd, char *buf)
{
	FILE * fp = fdopen(fd, "r");
	size_t len = 0;
	ssize_t read;
	buf = NULL;

	if ((read = getline(&buf, &len, fp)) != -1) {
		print("%s", buf);
		return 0;
	} else
		return -1;
}

int scp_read_packet(const char *packet_path, void *buf, int *buf_len)
{
	int fd, len, ret;
	buf = NULL;

	fd = open(packet_path, O_RDONLY);
	if (fd < 0) {
		print("%s - open packet path failed", __func__);
		return -1;
	}
	len = lseek(fd, 0, SEEK_END);
	print("%s - len = %d", __func__, len);
	buf = malloc(len);
	if (!buf) {
		print("%s - malloc failed, errno = %d", __func__, errno);
		return -1;
	}
	lseek(fd, 0, SEEK_SET);
	ret = read(fd, buf, len);
	if (ret != len) {
		print("%s - read buf failed, errno = %d", __func__, errno);
		free(buf);
		return -1;
	}
	*buf_len = ret;
	close(fd);

	return 0;
}

int scp_send(int serial_fd, void *buf, int buf_len)
{
	
}

int scp_update(int serial_fd, const char *packet_dir)
{
	FILE *fp;
	int list_fd;
	int len, size;
	char *packet_name = NULL;
	char packet_path[SCP_PACKET_PATH_MAX_SIZE];
	int retry_times = SCP_PACKET_SEND_RETRY_TIMES;
	void *packet_buf = NULL;
	void *buf = NULL;
	int buf_len;

	print("Start SCP session");
	fp = packet_list_open(packet_dir);
	if (!fp) {
		print("%s - open packet.list failed", __func__);
		return -1;
	}
	while ((len = getline(&packet_name, &size, fp)) != -1) {
		packet_name[len - 1] = '\0'; //remove '\n'
		//print("%s", packet_name);
		if (strstr(packet_name, SCP_PACKET_BL_EMULATION_MODE)) {
			// BL Emulation mode
		} else {
			// host mode
			packet_file_path_create(packet_dir, packet_name, packet_path, sizeof(packet_path));
			print("%s", packet_path);
			while (retry_times --) {
				if (scp_read_packet(packet_path, buf, &buf_len)) {
					print("%s - scp_read_packet failed", __func__);
					goto error_2;
				}
				if (serial_send(serial_fd, buf, buf_len) != buf_len) {
					print("%s - serial_send failed", __func__);
					goto error_1;
				}
				free(buf);
				print("%s - packet(%s) sends successed", __func__, packet_name);
			}
		}
	}

	packet_list_close(fp);
	free(packet_name);

	return 0;
error_1:
	free(buf);
error_2:
	free(packet_name);

	return -1;
}

int scp_burn(const char *device_name, const char *packet_dir)
{
	int serial_fd;

	serial_fd = serial_init(device_name);
	if (serial_fd < 0) {
		print("%s - serial_init failed", __func__);
		return -1;
	}
	scp_update(serial_fd, packet_dir);

	return 0;
}
