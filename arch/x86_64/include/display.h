#ifndef _DISPLAY_H
#define _DISPLAY_H

#include <stdint.h>
#include <stdarg.h>

void kprintf(const char *format, ...);
void display_init();

#endif