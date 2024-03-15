#include <stdint.h>
#include <stddef.h>

char* itoa(int32_t num, char* str, int base) {
    char* ptr = str;
    char* ptr1 = str;
    char tmp_char;
    int length = 0;
    
    if (num == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return str;
    }

    if (num < 0 && base == 10) {
        *ptr++ = '-';
        num = -num;
        ptr1++;
    }

    while (num != 0) {
        int32_t rem = num % base;
        *ptr++ = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
        length++;
    }

    *ptr = '\0';

    ptr--;
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }

    return str;
}

char* uitoa(uint32_t num, char* str, int base) {
    char* ptr = str;
    char* ptr1 = str;
    char tmp_char;
    int length = 0;
    
    if (num == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return str;
    }

    while (num != 0) {
        uint32_t rem = num % base;
        *ptr++ = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
        length++;
    }

    *ptr = '\0';

    ptr--;
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }

    return str;
}

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

void* memset(void* ptr, int value, size_t num) {
    unsigned char* p = ptr;
    while (num--) {
        *p++ = (unsigned char)value;
    }
    return ptr;
}