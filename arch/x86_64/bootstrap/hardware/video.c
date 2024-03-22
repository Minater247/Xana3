#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <multiboot2.h>
#include <video.h>
#include <serial.h>
#include <string.h>

void *video;
uint8_t framebuffer_type = 0;
uint32_t framebuffer_width = 0;
uint32_t framebuffer_height = 0;
uint32_t framebuffer_bpp = 0;
uint32_t framebuffer_pitch = 0;
struct multiboot_color *framebuffer_palette = NULL;
uint32_t framebuffer_palette_num_colors = 0;
uint32_t video_fg;
uint32_t video_bg;

struct psf1_header *header;
uint32_t char_pos_x = 0;
uint32_t char_pos_y = 0;
bool displayBackground = false;

void fb_putpixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT) {
        // set the character there to a space with background color
        uint16_t *video_memory = (uint16_t *)0xb8000;
        video_memory[80 * y + x] = (color << 12) | (video_fg << 8) | ' ';
        return;
    }

    switch (framebuffer_bpp)
    {
    case 8:
    {
        multiboot_uint8_t *pixel = video + framebuffer_pitch * y + x;
        *pixel = color;
    }
    break;
    case 15:
    case 16:
    {
        multiboot_uint16_t *pixel = video + framebuffer_pitch * y + 2 * x;
        *pixel = color;
    }
    break;
    case 24:
    {
        multiboot_uint32_t *pixel = video + framebuffer_pitch * y + 3 * x;
        *pixel = (color & 0xffffff) | (*pixel & 0xff000000);
    }
    break;

    case 32:
    {
        multiboot_uint32_t *pixel = video + framebuffer_pitch * y + 4 * x;
        *pixel = color;
    }
    break;
    }
}

uint32_t ega_color_defs[] = {
    0x00000000, // black
    0x000000aa, // blue
    0x0000aa00, // green
    0x0000aaaa, // cyan
    0x00aa0000, // red
    0x00aa00aa, // magenta
    0x00aa5500, // brown
    0x00aaaaaa, // light gray
    0x00555555, // dark gray
    0x005555ff, // light blue
    0x0055ff55, // light green
    0x0055ffff, // light cyan
    0x00ff5555, // light red
    0x00ff55ff, // light magenta
    0x00ffff55, // yellow
    0x00ffffff  // white
};

// Calculate the Euclidean distance between two colors in the RGB color space
unsigned color_distance(uint32_t color1, uint32_t color2) {
    unsigned r1 = (color1 >> 16) & 0xFF;
    unsigned g1 = (color1 >> 8) & 0xFF;
    unsigned b1 = color1 & 0xFF;

    unsigned r2 = (color2 >> 16) & 0xFF;
    unsigned g2 = (color2 >> 8) & 0xFF;
    unsigned b2 = color2 & 0xFF;

    unsigned dr = r1 - r2;
    unsigned dg = g1 - g2;
    unsigned db = b1 - b2;

    return dr * dr + dg * dg + db * db;
}

// Find the closest color in the palette, or just return the color if it's RGB
uint32_t framebuffer_get_color(uint32_t desired) {
    uint32_t closest = 0;

    switch (framebuffer_type)
    {
    case MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED:
    {
        unsigned best_distance = 4 * 256 * 256;
        struct multiboot_color *palette = framebuffer_palette;

        for (unsigned i = 0; i < framebuffer_palette_num_colors; i++)
        {
            unsigned distance = color_distance(desired, (palette[i].red << 16) | (palette[i].green << 8) | palette[i].blue);
            if (distance < best_distance)
            {
                closest = i;
                best_distance = distance;
            }
        }
    }
    break;

    case MULTIBOOT_FRAMEBUFFER_TYPE_RGB:
        closest = desired;
        break;

    case MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT:
    {
        unsigned best_distance = 4 * 256 * 256; 
        bool was_zero = false;
        for (unsigned i = 0; i < 16; i++)
        {
            unsigned distance = color_distance(desired, ega_color_defs[i]);

            if (distance < best_distance)
            {
                closest = i;
                best_distance = distance;
                if (desired == ega_color_defs[i]) {
                    was_zero = true;
                }
            } else if (!was_zero) {
                if (desired == ega_color_defs[i]) {
                    closest = i;
                    was_zero = true;
                }
            }
        }
        break;
    }

    default:
        closest = 0xffffffff;
        break;
    }

    return closest;
}

void video_setfg(uint32_t color)
{
    video_fg = framebuffer_get_color(color);
}

void video_setbg(uint32_t color)
{
    video_bg = framebuffer_get_color(color);
}

void enableBackground(bool enable) {
    displayBackground = enable;
}

void displayChar(char ch, uint32_t x, uint32_t y, uint32_t c, uint8_t chars[]) {
    uint8_t row;
    uint32_t x1 = x;
    for (uint32_t j = 0; j < header->fontheight; j++) {
        row = chars[ch * header->fontheight + j];
        for (uint32_t i = 0; i < FONT_WIDTH; i++) {
            if (i == FONT_WIDTH - 1) {
                if (displayBackground) {
                    fb_putpixel(x1, y, video_bg);
                }
                break;
            } else if (row & 0x80) {
                fb_putpixel(x1, y, c);
            } else if (displayBackground) {
                fb_putpixel(x1, y, video_bg);
            }
            row = row << 1;
            x1 = x1 + 1;
        }
        y = y + 1;
        x1 = x;
    }
}

extern struct psf1_header font_data_start;

bool video_init(struct multiboot_tag_framebuffer *tag)
{
    // Clear the screen
    for (uint32_t i = 0; i < framebuffer_height; i++)
    {
        for (uint32_t j = 0; j < framebuffer_width; j++)
        {
            fb_putpixel(j, i, video_bg);
        }
    }

    multiboot_uint32_t color;
    unsigned i;
    struct multiboot_tag_framebuffer *tagfb = (struct multiboot_tag_framebuffer *)tag;

    video = (void *)(uint32_t)tagfb->common.framebuffer_addr;
    framebuffer_type = tagfb->common.framebuffer_type;
    framebuffer_width = tagfb->common.framebuffer_width;
    framebuffer_height = tagfb->common.framebuffer_height;
    framebuffer_bpp = tagfb->common.framebuffer_bpp;
    framebuffer_pitch = tagfb->common.framebuffer_pitch;
    framebuffer_palette = tagfb->framebuffer_palette;
    framebuffer_palette_num_colors = tagfb->framebuffer_palette_num_colors;

    color = framebuffer_get_color(RGB2COLOR(0, 0, 0xff));

    for (i = 0; i < framebuffer_width && i < framebuffer_height; i++)
    {
        // now using fb_putpixel
        fb_putpixel(i, i, color);
    }

    // fill a rectangle
    // top left should lie on the line and be 1/8 of the way across
    // size should be 1/2 of the screen height
    for (i = framebuffer_width / 8; i < framebuffer_width / 8 + framebuffer_height / 2; i++)
    {
        for (unsigned j = framebuffer_width / 8; j < framebuffer_width / 8 + framebuffer_height / 2; j++)
        {
            fb_putpixel(i, j, color);
        }
    }

    serial_printf("Got framebuffer: %dx%d, %d bpp, pitch %d, type %d\n", framebuffer_width, framebuffer_height, framebuffer_bpp, framebuffer_pitch, framebuffer_type);

    // Initialize font data
    header = (struct psf1_header *)&font_data_start;

    video_fg = framebuffer_get_color(RGB2COLOR(0xff, 0xff, 0xff));
    video_bg = framebuffer_get_color(RGB2COLOR(0, 0, 0));

    return true;
}

void scroll(uint32_t n_pixels) { 
    if (framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT) {
        memcpy(video, video + n_pixels * framebuffer_pitch, (framebuffer_height - n_pixels) * framebuffer_pitch);
        memset(video + (framebuffer_height - n_pixels) * framebuffer_pitch, 0, n_pixels * framebuffer_pitch);
    } else {
        uint16_t *video_memory = (uint16_t *)video;
        for (uint32_t i = 0; i < framebuffer_height - n_pixels; i++) {
            for (uint32_t j = 0; j < framebuffer_width; j++) {
                video_memory[framebuffer_width * i + j] = video_memory[framebuffer_width * (i + n_pixels) + j];
            }
        }
        for (uint32_t i = framebuffer_height - n_pixels; i < framebuffer_height; i++) {
            for (uint32_t j = 0; j < framebuffer_width; j++) {
                video_memory[framebuffer_width * i + j] = (video_bg << 12) | (video_fg << 8) | ' ';
            }
        }
    }
}

void video_putc(char c)
{
    if (framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT) {
        if (c == '\n')
        {
            char_pos_x = 0;
            char_pos_y++;
        }
        else if (c == '\r')
        {
            char_pos_x = 0;
        }
        else
        {
            displayChar(c, char_pos_x * FONT_WIDTH, char_pos_y * header->fontheight + 1, video_fg, (uint8_t *)((uint32_t)&font_data_start + sizeof(struct psf1_header)));
            char_pos_x++;
        }

        // if we went off the edge, wrap
        if (char_pos_x >= framebuffer_width / FONT_WIDTH)
        {
            char_pos_x = 0;
            char_pos_y++;
        }

        if (char_pos_y >= framebuffer_height / header->fontheight)
        {
            scroll(8);
            char_pos_y--;
        }
    } else {
        uint16_t *video_memory = (uint16_t *)video;
        if (c == '\n')
        {
            char_pos_x = 0;
            char_pos_y++;
        }
        else if (c == '\r')
        {
            char_pos_x = 0;
        }
        else
        {
            video_memory[framebuffer_width * char_pos_y + char_pos_x] = (video_bg << 12) | (video_fg << 8) | c;
            char_pos_x++;
        }

        if (char_pos_x >= framebuffer_width)
        {
            char_pos_x = 0;
            char_pos_y++;
        }

        if (char_pos_y >= framebuffer_height)
        {
            scroll(1);
            char_pos_y--;
        }
    }
}