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

char *itoa64(int64_t num, char *str, int base) {
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
        int64_t rem = num % base;
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

char *uitoa64(uint64_t num, char *str, int base) {
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
        uint64_t rem = num % base;
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

void* memcpy(void* dest, const void* src, size_t n) {
    char* d = dest;
    const char* s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

//memmove
void* memmove(void* dest, const void* src, size_t n) {
    char* d = dest;
    const char* s = src;
    if (s < d && s + n > d) {
        s += n;
        d += n;
        while (n--) {
            *--d = *--s;
        }
    } else {
        while (n--) {
            *d++ = *s++;
        }
    }
    return dest;
}

//strcmp
int strcmp(const char* str1, const char* str2) {
    while (*str1 && *str1 == *str2) {
        str1++;
        str2++;
    }
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}

//strcat
char* strcat(char* dest, const char* src) {
    char* ptr = dest + strlen(dest);

    while (*src != '\0') {
        *ptr++ = *src++;
    }

    *ptr = '\0';

    return dest;
}

//strcpy
char* strcpy(char* dest, const char* src) {
    char* save = dest;
    while (*src) {
        *dest++ = *src++;
    }
    *dest = 0;
    return save;
}

//strncmp
int strncmp(const char* str1, const char* str2, size_t n) {
    while (n && *str1 && *str1 == *str2) {
        n--;
        str1++;
        str2++;
    }
    if (n == 0) {
        return 0;
    }
    return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}