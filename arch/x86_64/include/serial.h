#ifndef _SERIAL_H
#define _SERIAL_H

#include <stdint.h>
#include <stdarg.h>

#define SERIAL_COM1 0x3F8

#define SERIAL_DATA_PORT(base) (base)
#define SERIAL_FIFO_COMMAND_PORT(base) (base + 2)
#define SERIAL_LINE_COMMAND_PORT(base) (base + 3)
#define SERIAL_MODEM_COMMAND_PORT(base) (base + 4)
#define SERIAL_LINE_STATUS_PORT(base) (base + 5)

#define SERIAL_LINE_ENABLE_DLAB 0x80

void serial_init(uint16_t port, uint16_t baud_rate);
void serial_printf(const char *format, ...);
void serial_putchar(char a);

#endif