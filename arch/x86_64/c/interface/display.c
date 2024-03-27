#include <stdint.h>
#include <stdarg.h>

#include <display.h>
#include <video.h>
#include <string.h>

void puts(const char *s)
{
    for (size_t i = 0; s[i] != '\0'; i++)
    {
        video_putc(s[i]);
    }
}

void printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    char buffer[65];
    size_t len = strlen(format);
    for (size_t i = 0; i < len; i++)
    {
        if (format[i] == '%')
        {
            switch (format[++i])
            {
            case 'd':
                itoa(va_arg(args, int), buffer, 10);
                puts(buffer);
                break;
            case 's':
                puts(va_arg(args, char *));
                break;
            case 'c':
                video_putc(va_arg(args, int));
                break;
            case 'x':
                uitoa(va_arg(args, uint32_t), buffer, 16);
                puts(buffer);
                break;
            case 'l':
                if (format[i + 1] == 'x')
                {
                    uitoa64(va_arg(args, uint64_t), buffer, 16);
                    puts(buffer);
                    i++;
                } else if (format[i + 1] == 'd') {
                    itoa64(va_arg(args, int64_t), buffer, 10);
                    puts(buffer);
                    i++;
                }
                break;
            case '%':
                video_putc('%');
                break;
            default:
                video_putc(format[i]);
                break;
            }
        }
        else
        {
            video_putc(format[i]);
        }
    }
}