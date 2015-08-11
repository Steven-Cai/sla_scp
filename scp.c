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
#define SCP_PACKET_RECEIVE_MAX_SIZE 10240

#define SCP_PACKET_LIST_NAME "packet.list"
#define SCP_PACKET_BL_EMULATION_MODE "bl"
#define SCP_PACKET_HOST_MODE "host"
#define SCP_PACKET_SEND_RETRY_TIMES 15
#define SCP_PACKET_READ_TIMES 3
#define SCP_PACKET_TIMEOUT 2
#define SCP_PACKET_CMD_SIZE 8

struct scp_cmd_hdr {
	char sync1;
	char sync2;
	char sync3;
	unsigned char ctl;
	unsigned short dl;
	unsigned char id;
	unsigned char cks;
};

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

int scp_read_packet(const char *packet_path, void **buf, int *buf_len)
{
	int fd, len, ret;
	*buf = NULL;

	fd = open(packet_path, O_RDONLY);
	if (fd < 0) {
		print("%s - open packet path failed", __func__);
		return -1;
	}
	len = lseek(fd, 0, SEEK_END);
	print("%s - len = %d", __func__, len);
	*buf = malloc(len);
	if (!*buf) {
		print("%s - malloc failed, errno = %d", __func__, errno);
		return -1;
	}
	lseek(fd, 0, SEEK_SET);
	ret = read(fd, *buf, len);
	if (ret != len) {
		print("%s - read buf failed, errno = %d", __func__, errno);
		free(*buf);
		return -1;
	}
	*buf_len = ret;
	close(fd);

	return 0;
}

// FMT: '>cccBHBB'
void scp_parse_data(struct scp_cmd_hdr *scp_cmd_hdr, void *data)
{
	unsigned int scp_cmd[2] = {0};
	char *tmp = (char *)scp_cmd;

	memcpy(&scp_cmd, data, sizeof(scp_cmd));
	if (!is_big_endian()) {
		small_endian_2_big_endian(&scp_cmd[0]);
		small_endian_2_big_endian(&scp_cmd[1]);
	}
	scp_cmd_hdr->sync1 = (scp_cmd[0] & 0xff000000) >> 24;
	scp_cmd_hdr->sync2 = (scp_cmd[0] & 0x00ff0000) >> 16;
	scp_cmd_hdr->sync3 = (scp_cmd[0] & 0x0000ff00) >> 8;
	scp_cmd_hdr->ctl = (scp_cmd[0] & 0x000000ff);
	scp_cmd_hdr->dl = (scp_cmd[1] & 0xff000000) >> 24;
	scp_cmd_hdr->id = (scp_cmd[1] & 0x00ffff00) >> 8;
	scp_cmd_hdr->cks = (scp_cmd[1] & 0x000000ff);

	printf("sync1 = 0x%2x, sync2 = 0x%2x, sync3 = 0x%2x\n", scp_cmd_hdr->sync1, scp_cmd_hdr->sync2, scp_cmd_hdr->sync3);
	printf("ctl = %d, dl = %d, id = %d, cks = %d\n", scp_cmd_hdr->ctl, scp_cmd_hdr->dl, scp_cmd_hdr->id, scp_cmd_hdr->cks);
}

int scp_update(int serial_fd, const char *packet_dir)
{
	FILE *fp;
	int list_fd;
	int len, size;
	char *packet_name = NULL;
	char packet_path[SCP_PACKET_PATH_MAX_SIZE];
	int retry_times;
	void *packet_buf = NULL;
	void *packet_buf_pri = NULL;
	char receive_buf[SCP_PACKET_RECEIVE_MAX_SIZE];
	int buf_len, buf_len_pri;
	int ret = 0;
	char *tmp;
	int i;
	struct scp_cmd_hdr cmd_hdr;

	print("Start SCP session");
	fp = packet_list_open(packet_dir);
	if (!fp) {
		print("%s - open packet.list failed", __func__);
		return -1;
	}

	while ((len = getline(&packet_name, &size, fp)) != -1) {
		packet_name[len - 1] = '\0'; //remove '\n'
		//print("%s", packet_name);
		packet_file_path_create(packet_dir, packet_name, packet_path, sizeof(packet_path));
		retry_times = SCP_PACKET_READ_TIMES;
		while (retry_times) {
			retry_times --;
			if (scp_read_packet(packet_path, &packet_buf, &buf_len)) {
				print("%s - scp_read_packet(%s) failed, retry_times = %d", __func__, packet_name, retry_times);
				continue;
			} else {
				break;
			}
		}
		if (retry_times == 0) {
			print("%s - read packet %d times failed, giveup", __func__, SCP_PACKET_READ_TIMES);
			ret = -1;
			break;
		}
		// dispatch packet
		if (strstr(packet_name, SCP_PACKET_BL_EMULATION_MODE)) {
			// BL Emulation mode
			print("%s", packet_path);
			memset((void *)&cmd_hdr, 0, sizeof(struct scp_cmd_hdr));
			retry_times = SCP_PACKET_SEND_RETRY_TIMES;
			while (retry_times) {
				retry_times --;
				ret = serial_receive(serial_fd, receive_buf, sizeof(receive_buf));
				if (ret == -2) {
					if (serial_send(serial_fd, packet_buf_pri, buf_len_pri) != buf_len_pri)
						print("%s - serial send packet(%s) failed", __func__, packet_name);
					continue;
				} else if (ret == SCP_PACKET_CMD_SIZE) {
					tmp = receive_buf;
					for (i = 0; i < 8; i ++) {
						printf("%x  ", *(tmp ++));
					}
					putchar('\n');
					tmp = packet_buf;
					for (i = 0; i < 8; i ++)
						printf("%x  ", *(tmp ++));
					putchar('\n');
					if (!memcmp(receive_buf, packet_buf, buf_len)) {
						print("!!!!!!!!!!!!!!!!!!!!!!!!");
						scp_parse_data(&cmd_hdr, receive_buf);
						if (cmd_hdr.dl) {
							//BootloaderScp.py: 178
							ret = serial_receive(serial_fd, receive_buf, sizeof(receive_buf));
							if (ret > 0) {
								print("%s - cmd_hdr->dl  = %d, ret = %d\n", __func__, cmd_hdr.dl, ret);
							}
						}
						break;
					}
					else
						print("??????????????????");
				}
			}
		} else {
			// host mode
			print("%s", packet_path);
			retry_times = SCP_PACKET_SEND_RETRY_TIMES;
			while (retry_times) {
				retry_times --;
				if (serial_send(serial_fd, packet_buf, buf_len) != buf_len) {
					print("%s - serial send packet(%s) failed", __func__, packet_name);
					continue;
				}
				if (packet_buf_pri)
					free(packet_buf_pri);
				packet_buf_pri = calloc(1, buf_len);
				buf_len_pri = buf_len;
				memcpy(packet_buf_pri, packet_buf, buf_len);
				print("%s - packet(%s) sends successed", __func__, packet_name);
				free(packet_buf);
				break;
			}
			if (retry_times == 0) {
				free(packet_buf);
				ret = -1;
				break;
			}
		}
	}

	packet_list_close(fp);
	if (packet_name)
		free(packet_name);
	if (packet_buf_pri)
		free(packet_buf_pri);

	return ret;
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
