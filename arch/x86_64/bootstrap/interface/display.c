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
        else if (format[i] == '\033')
        {
            //read forward until \0 or a non-numeric/; character
            uint32_t end_i = i + 1;
            if (format[end_i] == '[')
            {
                end_i++;
            } else {
                // not a valid escape sequence
                video_putc(format[i]);
                continue;
            }
            while (((format[end_i] >= '0' && format[end_i] <= '9') || format[end_i] == ';') && format[end_i] != '\0')
            {
                end_i++;
            }
            //make sure we aren't at the end of the string
            if (format[end_i] == '\0') {
                break;
            }

            //depending on the character, process the sequence
            if (format[end_i] == 'm')
            {
                //color sequence, process each semi-colon separated number
                uint32_t start_i = i + 2;
                while (start_i < end_i)
                {
                    //read the number
                    uint32_t num = 0;
                    while (format[start_i] >= '0' && format[start_i] <= '9')
                    {
                        num *= 10;
                        num += format[start_i] - '0';
                        start_i++;
                    }
                    //process the number (this is annoyingly long but it works for now)
                    switch (num)
                    {
                    case 0:
                        //reset
                        video_setfg(RGB2COLOR(0xAA, 0xAA, 0xAA));
                        video_setbg(RGB2COLOR(0, 0, 0));
                        break;
                    case 1:
                        //bold (attribs not available in bootstrap)
                        break;
                    case 4:
                        //underline (attribs not available in bootstrap)
                        break;
                    case 30:
                        //black fg
                        video_setfg(RGB2COLOR(0, 0, 0));
                        break;
                    case 31:
                        //red fg
                        video_setfg(RGB2COLOR(0xAA, 0, 0));
                        break;
                    case 32:
                        //green fg
                        video_setfg(RGB2COLOR(0, 0xAA, 0));
                        break;
                    case 33:
                        //yellow fg
                        video_setfg(RGB2COLOR(0xAA, 0x55, 0));
                        break;
                    case 34:
                        //blue fg
                        video_setfg(RGB2COLOR(0, 0, 0xAA));
                        break;
                    case 35:
                        //magenta fg
                        video_setfg(RGB2COLOR(0xAA, 0, 0xAA));
                        break;
                    case 36:
                        //cyan fg
                        video_setfg(RGB2COLOR(0, 0xAA, 0xAA));
                        break;
                    case 37:
                        //white fg
                        video_setfg(RGB2COLOR(0xAA, 0xAA, 0xAA));
                        break;
                    case 40:
                        //black bg
                        video_setbg(RGB2COLOR(0, 0, 0));
                        break;
                    case 41:
                        //red bg
                        video_setbg(RGB2COLOR(0xAA, 0, 0));
                        break;
                    case 42:
                        //green bg
                        video_setbg(RGB2COLOR(0, 0xAA, 0));
                        break;
                    case 43:
                        //yellow bg
                        video_setbg(RGB2COLOR(0xAA, 0x55, 0));
                        break;
                    case 44:
                        //blue bg
                        video_setbg(RGB2COLOR(0, 0, 0xAA));
                        break;
                    case 45:
                        //magenta bg
                        video_setbg(RGB2COLOR(0xAA, 0, 0xAA));
                        break;
                    case 46:
                        //cyan bg
                        video_setbg(RGB2COLOR(0, 0xAA, 0xAA));
                        break;
                    case 47:
                        //white bg
                        video_setbg(RGB2COLOR(0xAA, 0xAA, 0xAA));
                        break;
                    case 90:
                        //light black fg
                        video_setfg(RGB2COLOR(0x55, 0x55, 0x55));
                        break;
                    case 91:
                        //light red fg
                        video_setfg(RGB2COLOR(0xFF, 0x55, 0x55));
                        break;
                    case 92:
                        //light green fg
                        video_setfg(RGB2COLOR(0x55, 0xFF, 0x55));
                        break;
                    case 93:
                        //light yellow fg
                        video_setfg(RGB2COLOR(0xFF, 0xFF, 0x55));
                        break;
                    case 94:
                        //light blue fg
                        video_setfg(RGB2COLOR(0x55, 0x55, 0xFF));
                        break;
                    case 95:
                        //light magenta fg
                        video_setfg(RGB2COLOR(0xFF, 0x55, 0xFF));
                        break;
                    case 96:
                        //light cyan fg
                        video_setfg(RGB2COLOR(0x55, 0xFF, 0xFF));
                        break;
                    case 97:
                        //light white fg
                        video_setfg(RGB2COLOR(0xFF, 0xFF, 0xFF));
                        break;
                    case 100:
                        //light black bg
                        video_setbg(RGB2COLOR(0x55, 0x55, 0x55));
                        break;
                    case 101:
                        //light red bg
                        video_setbg(RGB2COLOR(0xFF, 0x55, 0x55));
                        break;
                    case 102:
                        //light green bg
                        video_setbg(RGB2COLOR(0x55, 0xFF, 0x55));
                        break;
                    case 103:
                        //light yellow bg
                        video_setbg(RGB2COLOR(0xFF, 0xFF, 0x55));
                        break;
                    case 104:
                        //light blue bg
                        video_setbg(RGB2COLOR(0x55, 0x55, 0xFF));
                        break;
                    case 105:
                        //light magenta bg
                        video_setbg(RGB2COLOR(0xFF, 0x55, 0xFF));
                        break;
                    case 106:
                        //light cyan bg
                        video_setbg(RGB2COLOR(0x55, 0xFF, 0xFF));
                        break;
                    case 107:
                        //light white bg
                        video_setbg(RGB2COLOR(0xFF, 0xFF, 0xFF));
                        break;
                    }
                    start_i++;
                }
                //move the index to the end of the sequence
                i = end_i;
            }
        }
        else
        {
            video_putc(format[i]);
        }
    }
}