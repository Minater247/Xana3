#include <stdint.h>
#include <stddef.h>

#include <display.h>
#include <errors.h>
#include <multiboot.h>
#include <info.h>
#include <errors.h>
#include <status.h>
#include <serial.h>
#include <tables.h>
#include <drivers/PIT.h>

void preboot_load(uint32_t magic, void *mbd) {
    serial_init(SERIAL_COM1, 38400);
    serial_printf("XanaduOS preboot loader %s\n", VERSION_STRING);

    kassert(load_multiboot(magic, mbd));

    printf("XanaduOS preboot loader %s\n", VERSION_STRING);

    enableBackground(true);

    // ANSI color testing
    printf("\033[0;30ma\033[0;31mb\033[0;32mc\033[0;33md\033[0;34me\033[0;35mf\033[0;36mg\033[0;37mh\033[0m\n");
    printf("\033[0;90ma\033[0;91mb\033[0;92mc\033[0;93md\033[0;94me\033[0;95mf\033[0;96mg\033[0;97mh\033[0m\n");
    printf("\033[30;40ma\033[30;41mb\033[30;42mc\033[30;43md\033[30;44me\033[30;45mf\033[30;46mg\033[30;47mh\033[0m\n");
    printf("\033[30;100ma\033[30;101mb\033[30;102mc\033[30;103md\033[30;104me\033[30;105mf\033[30;106mg\033[30;107mh\033[0m\n");

    tables_initialize();

    keyboard_init();

    // ints on! We should be set to go now.
    asm volatile("sti");

    paging_init(); // sets up 64-bit paging but doesn't enable it

    STATUS("Nothing to do, waiting for kernel development ;)\n");

    kpanic("Testing panic\n");

    while (1);
}