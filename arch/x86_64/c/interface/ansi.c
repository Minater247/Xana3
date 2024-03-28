#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <video.h>
#include <string.h>
#include <memory.h>
#include <ansi.h>

bool in_escape_sequence = false;
char *escape_sequence = NULL;
uint32_t escape_sequence_length = 0;
uint64_t sequence_buffer_size = 0;

uint8_t state; // for multi-entry sequences

const uint8_t low_rgb[16][3] = {
    {0, 0, 0}, {128, 0, 0}, {0, 128, 0}, {128, 128, 0},
    {0, 0, 128}, {128, 0, 128}, {0, 128, 128}, {192, 192, 192},
    {128, 128, 128}, {255, 0, 0}, {0, 255, 0}, {255, 255, 0},
    {0, 0, 255}, {255, 0, 255}, {0, 255, 255}, {255, 255, 255}
};

void begin_escape_sequence()
{
    in_escape_sequence = true;
    escape_sequence = kmalloc(64);
    escape_sequence[0] = '\0';
    escape_sequence_length = 0;
    sequence_buffer_size = 64;
    state = ANSI_STATE_NONE;
}

uint32_t ansi_216_to_rgb(uint8_t ansi) {
    if (ansi < 16) {
        // If ANSI is less than 16, return the predefined low RGB values
        return RGB2COLOR(low_rgb[ansi][0], low_rgb[ansi][1], low_rgb[ansi][2]);
    } else if (ansi > 231) {
        // For ANSI codes greater than 231, generate grayscale values
        uint8_t s = (ansi - 232) * 10 + 8;
        return RGB2COLOR(s, s, s);
    } else {
        // For ANSI codes between 16 and 231, calculate RGB values
        uint8_t n = ansi - 16;
        uint8_t b = n % 6;
        uint8_t g = (n - b) / 6 % 6;
        uint8_t r = (n - b - g * 6) / 36 % 6;
        b = b ? b * 40 + 55 : 0;
        r = r ? r * 40 + 55 : 0;
        g = g ? g * 40 + 55 : 0;
        return RGB2COLOR(r, g, b);
    }
}

void handle_escape_sequence_color(uint32_t color)
{
    if (state == ANSI_STATE_NONE)
    {
        switch (color)
        {
        case 0:
            // reset
            video_setfg(RGB2COLOR(0xAA, 0xAA, 0xAA));
            video_setbg(RGB2COLOR(0, 0, 0));
            break;
        case 1:
            // bold (attribs TODO)
            break;
        case 4:
            // underline (attribs TODO)
            break;
        case 30:
            // black fg
            video_setfg(RGB2COLOR(0, 0, 0));
            break;
        case 31:
            // red fg
            video_setfg(RGB2COLOR(0xAA, 0, 0));
            break;
        case 32:
            // green fg
            video_setfg(RGB2COLOR(0, 0xAA, 0));
            break;
        case 33:
            // yellow fg
            video_setfg(RGB2COLOR(0xAA, 0x55, 0));
            break;
        case 34:
            // blue fg
            video_setfg(RGB2COLOR(0, 0, 0xAA));
            break;
        case 35:
            // magenta fg
            video_setfg(RGB2COLOR(0xAA, 0, 0xAA));
            break;
        case 36:
            // cyan fg
            video_setfg(RGB2COLOR(0, 0xAA, 0xAA));
            break;
        case 37:
            // white fg
            video_setfg(RGB2COLOR(0xAA, 0xAA, 0xAA));
            break;
        case 38:
            // extended fg
            state = ANSI_STATE_EXTENDED_FG;
            break;
        case 40:
            // black bg
            video_setbg(RGB2COLOR(0, 0, 0));
            break;
        case 41:
            // red bg
            video_setbg(RGB2COLOR(0xAA, 0, 0));
            break;
        case 42:
            // green bg
            video_setbg(RGB2COLOR(0, 0xAA, 0));
            break;
        case 43:
            // yellow bg
            video_setbg(RGB2COLOR(0xAA, 0x55, 0));
            break;
        case 44:
            // blue bg
            video_setbg(RGB2COLOR(0, 0, 0xAA));
            break;
        case 45:
            // magenta bg
            video_setbg(RGB2COLOR(0xAA, 0, 0xAA));
            break;
        case 46:
            // cyan bg
            video_setbg(RGB2COLOR(0, 0xAA, 0xAA));
            break;
        case 47:
            // white bg
            video_setbg(RGB2COLOR(0xAA, 0xAA, 0xAA));
            break;
        case 48:
            // extended bg
            state = ANSI_STATE_EXTENDED_BG;
            break;
        case 90:
            // light black fg
            video_setfg(RGB2COLOR(0x55, 0x55, 0x55));
            break;
        case 91:
            // light red fg
            video_setfg(RGB2COLOR(0xFF, 0x55, 0x55));
            break;
        case 92:
            // light green fg
            video_setfg(RGB2COLOR(0x55, 0xFF, 0x55));
            break;
        case 93:
            // light yellow fg
            video_setfg(RGB2COLOR(0xFF, 0xFF, 0x55));
            break;
        case 94:
            // light blue fg
            video_setfg(RGB2COLOR(0x55, 0x55, 0xFF));
            break;
        case 95:
            // light magenta fg
            video_setfg(RGB2COLOR(0xFF, 0x55, 0xFF));
            break;
        case 96:
            // light cyan fg
            video_setfg(RGB2COLOR(0x55, 0xFF, 0xFF));
            break;
        case 97:
            // light white fg
            video_setfg(RGB2COLOR(0xFF, 0xFF, 0xFF));
            break;
        case 100:
            // light black bg
            video_setbg(RGB2COLOR(0x55, 0x55, 0x55));
            break;
        case 101:
            // light red bg
            video_setbg(RGB2COLOR(0xFF, 0x55, 0x55));
            break;
        case 102:
            // light green bg
            video_setbg(RGB2COLOR(0x55, 0xFF, 0x55));
            break;
        case 103:
            // light yellow bg
            video_setbg(RGB2COLOR(0xFF, 0xFF, 0x55));
            break;
        case 104:
            // light blue bg
            video_setbg(RGB2COLOR(0x55, 0x55, 0xFF));
            break;
        case 105:
            // light magenta bg
            video_setbg(RGB2COLOR(0xFF, 0x55, 0xFF));
            break;
        case 106:
            // light cyan bg
            video_setbg(RGB2COLOR(0x55, 0xFF, 0xFF));
            break;
        case 107:
            // light white bg
            video_setbg(RGB2COLOR(0xFF, 0xFF, 0xFF));
            break;
        }
    } else if (state == ANSI_STATE_EXTENDED_FG) {
        if (color == 5) {
            state = ANSI_STATE_EXTENDED_FG_256;
        } else {
            state = ANSI_STATE_NONE;
        }
    } else if (state == ANSI_STATE_EXTENDED_BG) {
        if (color == 5) {
            state = ANSI_STATE_EXTENDED_BG_256;
        } else {
            state = ANSI_STATE_NONE;
        }
    } else if (state == ANSI_STATE_EXTENDED_FG_256) {
        video_setfg(ansi_216_to_rgb(color));
        state = ANSI_STATE_NONE;
    } else if (state == ANSI_STATE_EXTENDED_BG_256) {
        video_setbg(ansi_216_to_rgb(color));
        state = ANSI_STATE_NONE;
    }
}

void handleHome(char *seq) {
    // Go to home position if no arguments, or to the position specified if 2
    if (seq[1] == 'H') {
        video_set_cursor(0, 0);
    } else {
        // Parse the numbers
        char *where = seq;
        uint32_t row = 0;
        uint32_t col = 0;
        uint32_t i = 0;
        while (*where != 'H')
        {
            if (*where >= '0' && *where <= '9')
            {
                if (i == 0) {
                    row = row * 10 + (*where - '0');
                } else {
                    col = col * 10 + (*where - '0');
                }
            }
            else if (*where == ';')
            {
                i++;
            }
            where++;
        }
        video_set_cursor(row - 1, col - 1);
    }
}

void parse_escape_sequence(char *seq)
{
    // We will have something like \033[1;31m
    // We need to parse the numbers and the letter
    char command = seq[strlen(seq) - 1];

    switch (command)
    {
    case 'm':
    {
        // We have color codes, separated by ;
        char *where = seq;
        uint32_t color = 0;
        while (*where != 'm')
        {
            if (*where >= '0' && *where <= '9')
            {
                color = color * 10 + (*where - '0');
            }
            else if (*where == ';')
            {
                handle_escape_sequence_color(color);
                color = 0;
            }
            where++;
        }
        handle_escape_sequence_color(color);
        break;
    }
    case 'J':
    {
        // Clear screen. May have an argument, but defaults to 0
        if (seq[1] == '2')
        {
            fb_video_clear();
        } else if (seq[1] == '1') {
            fb_video_clear_up();
        } else {
            fb_video_clear_down();
        }
        break;
    }
    case 'H':
    {
        // Go to home position if no arguments, or to the position specified if 2
        handleHome(seq);
        break;
    }
    default:
    {
        // just write the escape sequence
        for (uint32_t i = 0; i < escape_sequence_length; i++)
        {
            video_putc(escape_sequence[i]);
        }
    }
    }
}

void handle_escape_sequence_char(char c)
{
    // if it's the first char and it's not [, it's not an escape sequence
    if (escape_sequence_length == 0)
    {
        if (c != '[')
        {
            in_escape_sequence = false;
            // just write the char
            video_putc('[');
            return;
        }
        else
        {
            // if length is too long, realloc
            if (escape_sequence_length >= sequence_buffer_size)
            {
                escape_sequence = krealloc(escape_sequence, escape_sequence_length + 64);
                sequence_buffer_size += 64;
            }

            escape_sequence[escape_sequence_length++] = c;
            escape_sequence[escape_sequence_length] = '\0';
            return;
        }
    }

    // if length is too long, realloc
    if (escape_sequence_length >= sequence_buffer_size)
    {
        escape_sequence = krealloc(escape_sequence, escape_sequence_length + 64);
        sequence_buffer_size += 64;
    }

    // if it's a number of ;, we're still in the sequence
    if ((c >= '0' && c <= '9') || c == ';')
    {
        escape_sequence[escape_sequence_length++] = c;
        escape_sequence[escape_sequence_length] = '\0';
    }
    else
    {
        // if it's not a number or ;, we're done
        in_escape_sequence = false;
        escape_sequence[escape_sequence_length++] = c;
        escape_sequence[escape_sequence_length] = '\0';
    }
}