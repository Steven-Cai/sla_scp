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
#define SCP_PACKET_BUF_MAX_SIZE 10240

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

FILE *packet_list_open(const char *packet_dir)
{
	FILE *fp = NULL;
	int dir_len = strlen(packet_dir);
	char *packet_list = SCP_PACKET_LIST_NAME;
	int list_len = strlen(packet_list);
	int max_path_size = dir_len + list_len + 1;
	char *packet_path = calloc(1, max_path_size);

	snprintf(packet_path, max_path_size, "%s%s", packet_dir, packet_list);
	print("%s - packet_path: %s", __func__, packet_path);
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

// *buf need to be free by manually
int scp_read_packet(const char *packet_path, void **buf, int *buf_len)
{
	int fd, len, ret;
	*buf = NULL;

	fd = open(packet_path, O_RDONLY);
	if (fd < 0) {
		print("%s - open packet path failed, errno = %d", __func__, errno);
		return -1;
	}
	len = lseek(fd, 0, SEEK_END);
	//print("%s - len = %d", __func__, len);
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
void scp_parse_cmd_hdr(struct scp_cmd_hdr *scp_cmd_hdr, void *data)
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

	//printf("sync1 = 0x%2x, sync2 = 0x%2x, sync3 = 0x%2x\n", scp_cmd_hdr->sync1, scp_cmd_hdr->sync2, scp_cmd_hdr->sync3);
	//printf("ctl = %d, dl = %d, id = %d, cks = %d\n", scp_cmd_hdr->ctl, scp_cmd_hdr->dl, scp_cmd_hdr->id, scp_cmd_hdr->cks);
}

int scp_packet_receive(int fd, char *buf, int expect_size)
{
	int ret, size = 0;
	char *index = buf;

	//print("%s - expect_size = %d", __func__, expect_size);
	while (size != expect_size) {
		ret = serial_receive(fd, index, expect_size - size);
		if (ret < 0) {
			print("%s - serial_receive failed, return = %d", __func__, ret);
			return ret;
		}
		size += ret;
		if (size == expect_size) {
			//print("%s - read length = %d", __func__, size);
			return size;
		} else if (size < expect_size) {
			index += size;
		} else {
			print("%s - Impossible is nothing", __func__);
			return -1;
		}
	}

	return size;
}

int scp_update(int serial_fd, const char *packet_dir)
{
	FILE *fp;
	int list_fd;
	int len, size;
	char *packet_name = NULL;
	void *packet_buf = NULL;
	void *packet_buf_pre = NULL;
	char packet_path[SCP_PACKET_PATH_MAX_SIZE];
	char bl_buf[SCP_PACKET_BUF_MAX_SIZE];
	int packet_len, packet_len_pre;
	int retry_times;
	int ret = 0;
	char *tmp;
	int i;
	struct scp_cmd_hdr bl_cmd_hdr;
	struct scp_cmd_hdr packet_cmd_hdr;

	print("Start SCP session");
	fp = packet_list_open(packet_dir);
	if (!fp) {
		print("%s - open packet.list failed, giveup", __func__);
		return -1;
	}

	while ((len = getline(&packet_name, &size, fp)) != -1) {
		// packet_name need to be free manually
		packet_name[len - 1] = '\0'; //remove '\n'
		snprintf(packet_path, sizeof(packet_path), "%s%s", packet_dir, packet_name);
		retry_times = SCP_PACKET_READ_TIMES; // 3
		// read packet file form buildapp in the local directory
		while (retry_times) {
			retry_times --;
			if (scp_read_packet(packet_path, &packet_buf, &packet_len))
				print("%s - scp_read_packet(%s) failed, retry_times = %d", __func__, packet_name, SCP_PACKET_READ_TIMES - retry_times);
			else
				break;
		}
		if (retry_times == 0) {
			print("%s - read packet %d times failed, giveup", __func__, SCP_PACKET_READ_TIMES);
			ret = -1;
			goto error_1;;
		}
		// dispatch packet
		if (strstr(packet_name, SCP_PACKET_BL_EMULATION_MODE)) {
			// BL mode
#ifdef DEBUG
			print("wait < %s", packet_path);
#endif
			memset((void *)&bl_cmd_hdr, 0, sizeof(struct scp_cmd_hdr));
			memset((void *)&packet_cmd_hdr, 0, sizeof(struct scp_cmd_hdr));
			retry_times = SCP_PACKET_SEND_RETRY_TIMES;
			while (retry_times) {
				retry_times --;
				// receive bl packet from MAX32550
				ret = scp_packet_receive(serial_fd, bl_buf, packet_len);
				if (ret == -2) {
					// receive timeout
					if (serial_send(serial_fd, packet_buf_pre, packet_len_pre) != packet_len_pre)
						print("%s - serial resend previous packet(%s) failed", __func__, packet_name);
					else
						print("%s - serial resend previous packet(%s)", __func__, packet_name);
					continue;
				} else if (ret < 0) {
					print("%s - scp_packet_receive error, ret = %d", __func__, ret);
					ret = -1;
					goto error_2;
				}
				if (ret == packet_len) {
					scp_parse_cmd_hdr(&packet_cmd_hdr, packet_buf);
					if (!memcmp(bl_buf, packet_buf, packet_len)) {
						free(packet_buf);
						packet_buf = NULL;
#ifdef DEBUG
						print("bl packet is identical with the packet from MAX32550\n");
#endif
						break;
					} else {
						free(packet_buf);
						packet_buf = NULL;
						print("bl packet is different with the packet from MAX32550\n");
						scp_parse_cmd_hdr(&bl_cmd_hdr, bl_buf);
						if (bl_cmd_hdr.dl) {
							//BootloaderScp.py: 178
							ret = serial_receive(serial_fd, bl_buf, sizeof(bl_buf));
							if (ret > 0)
								print("%s - bl_cmd_hdr->dl  = %d, ret = %d\n", __func__, bl_cmd_hdr.dl, ret);
						}
						if (!memcmp(&packet_cmd_hdr, &bl_cmd_hdr, sizeof(struct scp_cmd_hdr))) {
							print("packet_cmd_hdr and bl_cmd_hdr have the same cmd_hdr, although the data is different, PASS");
							break;
						} else {
							print("%s - error bl packet, giveup", __func__);
							ret = -1;
							goto error_2;
						}
					}
				}
			}
			if (retry_times == 0) {
				print("%s - receive packet(%s) %d times failed", __func__, packet_name, SCP_PACKET_SEND_RETRY_TIMES);
				ret = -1;
				goto error_2;
			}
		} else {
			// host mode
#ifdef DEBUG
			print("send > %s", packet_path);
#endif
			retry_times = SCP_PACKET_SEND_RETRY_TIMES;
			while (retry_times) {
				retry_times --;
				if (serial_send(serial_fd, packet_buf, packet_len) != packet_len) {
					print("%s - serial send packet(%s) failed", __func__, packet_name);
					continue;
				}
				// serial_send success
				if (packet_buf_pre)
					free(packet_buf_pre);
				packet_buf_pre = calloc(1, packet_len);
				packet_len_pre = packet_len;
				memcpy(packet_buf_pre, packet_buf, packet_len);
				free(packet_buf);
				packet_buf = NULL;
#ifdef DEBUG
				print("%s - packet(%s) sends successed\n", __func__, packet_name);
#endif
				break;
			}
			if (retry_times == 0) {
				print("%s - send packet(%s) %d times failed", __func__, packet_name, SCP_PACKET_SEND_RETRY_TIMES);
				ret = -1;
				goto error_2;
			}
		}
	}
	print("SCP session success");
	ret = 0;

error_2:
	if (packet_buf)
		free(packet_buf);
	if (packet_buf_pre)
		free(packet_buf_pre);
error_1:
	if (packet_name)
		free(packet_name);
	packet_list_close(fp);

	return ret;
}

int scp_send(const char *device_name, const char *packet_dir)
{
	int serial_fd, ret;

	serial_fd = serial_init(device_name);
	if (serial_fd < 0) {
		print("%s - serial_init failed", __func__);
		return -1;
	}

	ret = scp_update(serial_fd, packet_dir);

	serial_close(serial_fd);

	return ret;
}
