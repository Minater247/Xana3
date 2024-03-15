#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

char* itoa(int32_t num, char* str, int base);
char* uitoa(uint32_t num, char* str, int base);
size_t strlen(const char* str);
void* memset(void* ptr, int value, size_t num);

#endif