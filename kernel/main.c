#include <stdint.h>

void kmain() {
    // Loop indefinitely
    while (1) {}
}

void __init() {
    // We still need a jump up from identity-mapped memory
    uint64_t kmain_addr = (uint64_t)kmain;
    asm volatile("jmp *%0" : : "r"(kmain_addr));
}