#include <stdint.h>

#include <system.h>

void outb(uint16_t port, uint8_t value) {
    ASM_OUTB(port, value);
}

uint8_t inb(uint16_t port) {
    uint8_t ret;
    ASM_INB(port, ret);
    return ret;
}