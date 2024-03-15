#ifndef _VIDEO_H
#define _VIDEO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <multiboot2.h>

#define RGB2COLOR(r, g, b) ((r << 16) | (g << 8) | b)

bool video_init(struct multiboot_tag_framebuffer *tag);
void video_setfg(uint32_t color);
void video_setbg(uint32_t color);
void video_putc(char c);

#endif