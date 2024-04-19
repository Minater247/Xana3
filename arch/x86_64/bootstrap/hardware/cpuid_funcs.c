#include <stdint.h>
#include <stdbool.h>
#include <cpuid.h>

unsigned int eax, ebx, ecx, edx;

bool has_cpuid() {
    return __get_cpuid(0, &eax, &ebx, &ecx, &edx);
}

bool has_long_mode() {
    eax = 0;
    __get_cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    if (eax < 0x80000001) {
        return false;
    }
    __get_cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
    return edx & (1 << 29);
}

bool has_1gb_pages() {
    eax = 0;
    __get_cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    if (eax < 0x80000001) {
        return false;
    }
    __get_cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
    return edx & (1 << 26);
}

bool has_sse() {
    eax = 0;
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);
    return edx & (1 << 25);
}