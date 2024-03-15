#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <multiboot2.h>
#include <video.h>

void *video;
uint8_t framebuffer_type = 0;
uint32_t framebuffer_width = 0;
uint32_t framebuffer_height = 0;
uint32_t framebuffer_bpp = 0;
uint32_t framebuffer_pitch = 0;
struct multiboot_color *framebuffer_palette = NULL;
uint32_t framebuffer_palette_num_colors = 0;
uint32_t video_fg = 0xffffffff;
uint32_t video_bg = 0x00000000;

void fb_putpixel(int x, int y, uint32_t color)
{
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

// Find the closest color in the palette, or just return the color if it's RGB
uint32_t framebuffer_get_color(uint32_t desired) {
    uint32_t closest = 0;

    switch (framebuffer_type)
    {
    case MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED:
    {
        unsigned best_distance = 4 * 256 * 256;
        unsigned distance;
        struct multiboot_color *palette = framebuffer_palette;

        for (unsigned i = 0; i < framebuffer_palette_num_colors; i++)
        {
            distance = (desired - palette[i].blue) * (desired - palette[i].blue) + palette[i].red * palette[i].red + palette[i].green * palette[i].green;
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

bool video_init(struct multiboot_tag_framebuffer *tag)
{


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

    color = framebuffer_get_color(RGB2COLOR(0xff, 0xff, 0xff));

    // for (i = 0; i < framebuffer_width && i < framebuffer_height; i++)
    // {
    //     // now using fb_putpixel
    //     fb_putpixel(i, i, color);
    // }

    // // fill a rectangle
    // // top left should lie on the line and be 1/8 of the way across
    // // size should be 1/2 of the screen height
    // for (i = framebuffer_width / 8; i < framebuffer_width / 8 + framebuffer_height / 2; i++)
    // {
    //     for (unsigned j = framebuffer_width / 8; j < framebuffer_width / 8 + framebuffer_height / 2; j++)
    //     {
    //         fb_putpixel(i, j, color);
    //     }
    // }

    // as a test, write the letter A to the top left of the screen
    // we will make a font later
    for (i = 0; i < 8; i++)
    {
        for (unsigned j = 0; j < 8; j++)
        {
            if (i == 0 || i == 7 || j == 0 || j == 3)
            {
                fb_putpixel(i, j, color);
            }
        }
    }

    return true;
}

uint32_t char_pos_x = 0;
uint32_t char_pos_y = 0;

void video_putc(char c)
{
    if (c == '\n')
    {
        char_pos_x = 0;
        char_pos_y++;
    }
    else
    {
        //fb_putpixel(char_pos_x, char_pos_y, video_fg);
        char_pos_x++;
    }
}