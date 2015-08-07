#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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
