#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

char* itoa(int32_t num, char* str, int base);
char* uitoa(uint32_t num, char* str, int base);\
char *itoa64(int64_t num, char *str, int base);
char *uitoa64(uint64_t num, char *str, int base);
size_t strlen(const char* str);
void* memset(void* ptr, int value, size_t num);
void* memcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int strcmp(const char* str1, const char* str2);
char* strcat(char* dest, const char* src);
char* strcpy(char* dest, const char* src);
int strncmp(const char* str1, const char* str2, size_t n);
int64_t atoi(const char* str);

#endif