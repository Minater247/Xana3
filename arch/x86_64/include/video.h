#ifndef _VIDEO_H
#define _VIDEO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <multiboot2.h>
#include <device.h>

struct psf1_header {
    uint8_t magic[2];
    uint8_t filemode;
    uint8_t fontheight;
};

#define RGB2COLOR(r, g, b) ((r << 16) | (g << 8) | b)

#define FONT_WIDTH 9

bool video_init(struct multiboot_tag_framebuffer *tag);
void video_setfg(uint32_t color);
void video_setbg(uint32_t color);
void video_putc(char c);
void video_puts(const char *s, int x, int y, uint32_t c, uint8_t chars[]);
void enableBackground(bool enable);
void fb_video_clear();
void fb_video_clear_up();
void fb_video_clear_down();
void video_set_cursor(uint32_t x, uint32_t y);
device_t *init_fb_device();

#endif