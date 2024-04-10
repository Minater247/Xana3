#ifndef _IOCTL_H
#define _IOCTL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define FBIOGET_VSCREENINFO 0x4600 // 0x46 = 'F', 0x00 = VSCREENINFO (get)

struct fb_var_screeninfo {
    uint32_t xres; // visible resolution
    uint32_t yres;
    uint32_t xres_virtual; // virtual resolution
    uint32_t yres_virtual;
    uint32_t xoffset; // offset from virtual to visible resolution
    uint32_t yoffset;

    uint32_t bits_per_pixel; // bits per pixel
    uint32_t grayscale; // 0 = color, 1 = grayscale

    uint64_t pitch; // length of a line in bytes

    // other stuff to be defined later
};

#endif