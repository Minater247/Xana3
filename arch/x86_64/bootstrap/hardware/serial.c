#include <stdint.h>
#include <stdarg.h>
#include <io.h>
#include <string.h>
#include <serial.h>
#include <stddef.h>

void serial_init(uint16_t port, uint16_t baud_rate) {
    outb(SERIAL_LINE_COMMAND_PORT(port), SERIAL_LINE_ENABLE_DLAB); // Enable DLAB (set baud rate divisor)
    outb(SERIAL_DATA_PORT(port), (baud_rate >> 8) & 0x00FF); // Set divisor to 3 (lo byte) 38400 baud
    outb(SERIAL_DATA_PORT(port), baud_rate & 0x00FF); //                  (hi byte)
    outb(SERIAL_LINE_COMMAND_PORT(port), 0x03); // 8 bits, no parity, one stop bit
}

int serial_transmit_empty(uint16_t port) {
    return inb(SERIAL_LINE_STATUS_PORT(port)) & 0x20;
}

void serial_write(uint16_t port, char a) {
    while (!serial_transmit_empty(port));
    outb(port, a);
}

void serial_writestring(const char *str) {
    for (uint32_t i = 0; i < strlen(str); i++) {
        serial_write(SERIAL_COM1, str[i]);
    }
}

void serial_putchar(char a)
{
    serial_write(SERIAL_COM1, a);
}



void serial_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    char buffer[32];
    size_t len = strlen(format);
    for (size_t i = 0; i < len; i++)
    {
        if (format[i] == '%')
        {
            switch (format[++i])
            {
            case 'd':
                itoa(va_arg(args, int), buffer, 10);
                serial_writestring(buffer);
                break;
            case 's':
                serial_writestring(va_arg(args, char *));
                break;
            case 'c':
                serial_putchar(va_arg(args, int));
                break;
            case 'x':
                uitoa(va_arg(args, uint32_t), buffer, 16);
                serial_writestring(buffer);
                break;
            case '%':
                serial_putchar('%');
                break;
            default:
                serial_putchar(format[i]);
                break;
            }
        }
        else
        {
            serial_putchar(format[i]);
        }
    }
}