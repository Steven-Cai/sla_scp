#ifndef __UTILES_H
#define __UTILES_H

#include <asm-generic/errno-base.h>

void print(const char *fmt, ...);

#define GPIO_PATH_MAX_SIZE 1024
#define GPIO_PREFIX_PATH "/sys/class/gpio"

#endif
