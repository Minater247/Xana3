#include <stdint.h>
#include <stddef.h>

#include <io.h>
#include <display.h>
#include <system.h>
#include <tables.h>
#include <unused.h>

#define PS2_GET_COMPAQ_STATUS 0x20
#define PS2_SET_COMPAQ_STATUS 0x60
#define PS2_ENABLE_AUX_PORT 0xA8
#define PS2_ADDRESS_MOUSE 0xD4

#define MOUSE_PORT_DATA 0x60
#define MOUSE_PORT_STATUS 0x64

#define MOUSE_ENABLE_PACKETS 0xF4
#define MOUSE_COMMAND_DEFAULTS 0xF6

#define MOUSE_TIMEOUT 100000

void mouse_wait_send() {
    uint32_t timeout = MOUSE_TIMEOUT;
    while (--timeout) {
        if (!((inb(MOUSE_PORT_STATUS) & 0x02))) {
            return;
        }
    }
}

void mouse_wait_receive() {
    uint32_t timeout = MOUSE_TIMEOUT;
    while (--timeout) {
        if ((inb(MOUSE_PORT_STATUS) & 0x01) == 1) {
            return;
        }
    }
}

void mouse_send(uint8_t write) {
    mouse_wait_send();
    outb(MOUSE_PORT_STATUS, PS2_ADDRESS_MOUSE);
    mouse_wait_send();
    outb(MOUSE_PORT_DATA, write);
}

uint8_t mouse_receive() {
    mouse_wait_receive();
    return inb(MOUSE_PORT_DATA);
}

void mouse_handler(regs_t *r) {
    UNUSED(r);
    
    // Read the mouse data
    uint8_t bytes[3];
    for (int i = 0; i < 3; i++) {
        bytes[i] = mouse_receive();
    }
    // check if either bit 3 is unset (not valid) or bits 6 and 7 are set (weird)
    if (!(bytes[0] & 0x8) || (bytes[0] & 0xC0)) {
        return;
    }

    // Print the mouse data
    // kprintf("Mouse movement: x=%d, y=%d\n", (int8_t)bytes[1], (int8_t)bytes[2]);
}

void mouse_init() {
    uint8_t status_byte;

    // Enable the mouse
    mouse_wait_send();
    outb(MOUSE_PORT_STATUS, PS2_ENABLE_AUX_PORT);

    // Fetch the current status byte
    mouse_wait_send();
    outb(MOUSE_PORT_STATUS, PS2_GET_COMPAQ_STATUS);
    mouse_wait_receive();
    status_byte = inb(MOUSE_PORT_DATA) | 2;

    // Set the new status byte
    mouse_wait_send();
    outb(MOUSE_PORT_STATUS, PS2_SET_COMPAQ_STATUS);
    mouse_wait_send();
    outb(MOUSE_PORT_DATA, status_byte);

    // Set up packet streaming at default settings
    mouse_send(MOUSE_COMMAND_DEFAULTS);
    mouse_receive();
    mouse_send(MOUSE_ENABLE_PACKETS);
    mouse_receive();

    register_interrupt_handler(12, mouse_handler);
}