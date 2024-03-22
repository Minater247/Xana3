#include <stdint.h>
#include <stddef.h>

#include <io.h>

#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_DATA_PORT 0x60

#define KEYBOARD_ACK 0xFA

int keyboard_send_command(uint8_t command) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if (!(inb(KEYBOARD_STATUS_PORT) & 0x2)) {
            outb(KEYBOARD_DATA_PORT, command);
            return 0;
        }
    }
    return -1; // Timeout
}

int keyboard_wait_ack() {
    uint32_t timeout = 100000;
    while (timeout--) {
        if (inb(KEYBOARD_DATA_PORT) == KEYBOARD_ACK) {
            return 0;
        }
    }
    return -1; // Timeout
}

void keyboard_toggle_caps_lock() {
    static uint8_t caps_lock_on = 0;
    if (keyboard_send_command(0xED) == 0 && keyboard_wait_ack() == 0) {
        caps_lock_on = !caps_lock_on;
        if (keyboard_send_command(caps_lock_on ? 0x4 : 0x0) == 0) {
            keyboard_wait_ack();
        }
    }
}

void keyboard_init() {
    if (keyboard_send_command(0xF4) == 0) {
        keyboard_wait_ack();
    }
}

int count = 100;
void panic_flash() {
    count--;
    if (count == 0) {
        count = 100;
        keyboard_toggle_caps_lock();
    }
}