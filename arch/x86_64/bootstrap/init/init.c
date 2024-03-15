#include <stdint.h>

#include <display.h>
#include <errors.h>
#include <multiboot.h>
#include <info.h>
#include <errors.h>
#include <status.h>
#include <serial.h>

void preboot_load(uint32_t magic, void *mbd) {
    serial_init(SERIAL_COM1, 38400);
    serial_printf("XanaduOS preboot loader %s\n", VERSION_STRING);

    kassert(load_multiboot(magic, mbd));

    printf("XanaduOS preboot loader %s\n", VERSION_STRING);

    STATUS("Nothing to do, waiting for kernel development ;)");

    while (1);
}