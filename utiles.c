#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

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
