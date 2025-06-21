#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <multiboot2.h>
#include <video.h>
#include <serial.h>
#include <string.h>
#include <system.h>
#include <device.h>
#include <memory.h>
#include <unused.h>
#include <errors.h>
#include <ansi.h>
#include <filesystem.h>
#include <sys/ioctl.h>
#include <sys/errno.h>

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

uint64_t user_pitch;

struct psf1_header *header;
uint32_t char_pos_x = 0;
uint32_t char_pos_y = 0;
bool displayBackground = false;

void fb_putpixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT)
    {
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

void fb_video_fillrect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    if (framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT)
    {
        uint16_t *video_memory = (uint16_t *)video;
        for (uint32_t i = 0; i < h; i++)
        {
            for (uint32_t j = 0; j < w; j++)
            {
                video_memory[framebuffer_width * (y + i) + x + j] = (color << 12) | (video_fg << 8) | ' ';
            }
        }
        return;
    }

    switch (framebuffer_bpp)
    {
    case 8:
    {
        multiboot_uint8_t *pixel = video + framebuffer_pitch * y + x;
        for (uint32_t i = 0; i < h; i++)
        {
            for (uint32_t j = 0; j < w; j++)
            {
                *pixel = color;
                pixel++;
            }
            pixel += framebuffer_pitch - w;
        }
        break;
    }
    case 15:
    case 16:
    {
        multiboot_uint16_t *pixel = video + framebuffer_pitch * y + 2 * x;
        for (uint32_t i = 0; i < h; i++)
        {
            for (uint32_t j = 0; j < w; j++)
            {
                *pixel = color;
                pixel++;
            }
            pixel += framebuffer_pitch / 2 - w;
        }
        break;
    }
    case 24:
    {
        multiboot_uint8_t *pixel = video + framebuffer_pitch * y + 3 * x;
        for (uint32_t i = 0; i < h; i++)
        {
            for (uint32_t j = 0; j < w; j++)
            {
                pixel[0] = color & 0xff;         // Blue component
                pixel[1] = (color >> 8) & 0xff;  // Green component
                pixel[2] = (color >> 16) & 0xff; // Red component
                pixel += 3;
            }
            pixel += framebuffer_pitch - 3 * w;
        }
        break;
    }
    case 32:
    {
        multiboot_uint32_t *pixel = video + framebuffer_pitch * y + 4 * x;
        for (uint32_t i = 0; i < h; i++)
        {
            for (uint32_t j = 0; j < w; j++)
            {
                *pixel = color;
                pixel++;
            }
            pixel += framebuffer_pitch / 4 - w;
        }
        break;
    }
    }
}

void fb_video_clearto_bpp32(uint32_t color)
{
    // much easier than fillrect, we can just use memset
    memset(video, color, framebuffer_pitch * framebuffer_height);
}

void fb_video_clearto_bpp24(uint32_t color)
{
    for (uint32_t i = 0; i < framebuffer_height; i++)
    {
        for (uint32_t j = 0; j < framebuffer_width; j++)
        {
            multiboot_uint32_t *pixel = video + framebuffer_pitch * i + 3 * j;
            *pixel = (color & 0xffffff) | (*pixel & 0xff000000);
        }
    }
}

void fb_video_clearto_bpp16(uint32_t color)
{
    for (uint32_t i = 0; i < framebuffer_height; i++)
    {
        for (uint32_t j = 0; j < framebuffer_width; j++)
        {
            multiboot_uint16_t *pixel = video + framebuffer_pitch * i + 2 * j;
            *pixel = color;
        }
    }
}

void fb_video_clearto_bpp8(uint32_t color)
{
    for (uint32_t i = 0; i < framebuffer_height; i++)
    {
        for (uint32_t j = 0; j < framebuffer_width; j++)
        {
            multiboot_uint8_t *pixel = video + framebuffer_pitch * i + j;
            *pixel = color;
        }
    }
}

void fb_video_clearto(uint32_t color)
{
    if (framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT)
    {
        uint16_t *video_memory = (uint16_t *)video;
        for (uint32_t i = 0; i < framebuffer_height; i++)
        {
            for (uint32_t j = 0; j < framebuffer_width; j++)
            {
                video_memory[framebuffer_width * i + j] = (color << 12) | (video_fg << 8) | ' ';
            }
        }
        return;
    }

    switch (framebuffer_bpp)
    {
    case 8:
        fb_video_clearto_bpp8(color);
        break;
    case 15:
    case 16:
        fb_video_clearto_bpp16(color);
        break;
    case 24:
        fb_video_clearto_bpp24(color);
        break;
    case 32:
        fb_video_clearto_bpp32(color);
        break;
    }
}

void fb_video_clear()
{
    fb_video_clearto(video_bg);
}

void fb_video_clear_up() {
    // ANSI 1J
    // Clear from cursor to beginning of screen

    // step 1: clear from cursor to beginning of line (rectangle 1)
    if (framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT)
    {
        fb_video_fillrect(0, char_pos_y * header->fontheight, (char_pos_x + 1) * FONT_WIDTH, header->fontheight, video_bg);
    }
    else
    {
        uint16_t *video_memory = (uint16_t *)video;
        for (uint32_t x = 0; x <= char_pos_x; x++)
        {
            video_memory[framebuffer_width * char_pos_y + x] = (video_bg << 12) | (video_fg << 8) | ' ';
        }
    }

    // step 2: clear from beginning of screen to cursor (rectangle 2)
    if (framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT)
    {
        fb_video_fillrect(0, 0, framebuffer_width, char_pos_y * header->fontheight, video_bg);
    }
    else
    {
        uint16_t *video_memory = (uint16_t *)video;
        for (uint32_t i = 0; i < char_pos_y; i++)
        {
            for (uint32_t j = 0; j < framebuffer_width; j++)
            {
                video_memory[framebuffer_width * i + j] = (video_bg << 12) | (video_fg << 8) | ' ';
            }
        }
    }
}

void video_set_cursor(uint32_t x, uint32_t y)
{
    // move the cursor to the specified position
    char_pos_x = x;
    char_pos_y = y;
}

void fb_video_clear_down() {
    // ANSI 0J
    // Clear from cursor to end of screen

    // step 1: clear from cursor to end of line (rectangle 1)
    if (framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT)
    {
        fb_video_fillrect(char_pos_x * FONT_WIDTH, char_pos_y * header->fontheight, framebuffer_width - char_pos_x * FONT_WIDTH, header->fontheight, video_bg);
    }
    else
    {
        uint16_t *video_memory = (uint16_t *)video;
        for (uint32_t x = char_pos_x; x < framebuffer_width; x++)
        {
            video_memory[framebuffer_width * char_pos_y + x] = (video_bg << 12) | (video_fg << 8) | ' ';
        }
    }

    // step 2: clear from next line to end of screen (rectangle 2)
    if (framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT)
    {
        fb_video_fillrect(0, (char_pos_y + 1) * header->fontheight, framebuffer_width, framebuffer_height - (char_pos_y + 1) * header->fontheight, video_bg);
    }
    else
    {
        uint16_t *video_memory = (uint16_t *)video;
        for (uint32_t i = char_pos_y + 1; i < framebuffer_height / header->fontheight; i++)
        {
            for (uint32_t j = 0; j < framebuffer_width; j++)
            {
                video_memory[framebuffer_width * i + j] = (video_bg << 12) | (video_fg << 8) | ' ';
            }
        }
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
unsigned color_distance(uint32_t color1, uint32_t color2)
{
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
uint32_t framebuffer_get_color(uint32_t desired)
{
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
                if (desired == ega_color_defs[i])
                {
                    was_zero = true;
                }
            }
            else if (!was_zero)
            {
                if (desired == ega_color_defs[i])
                {
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

void enableBackground(bool enable)
{
    displayBackground = enable;
}

void displayChar(char ch, uint32_t x, uint32_t y, uint32_t c, uint8_t chars[])
{
    uint8_t row;
    uint32_t x1 = x;

    // We don't support negative characters
    if (ch < 0) {
        ch = 0;
    }

    for (uint32_t j = 0; j < header->fontheight; j++)
    {
        row = chars[ch * header->fontheight + j];
        for (uint32_t i = 0; i < FONT_WIDTH; i++)
        {
            if (i == FONT_WIDTH - 1)
            {
                if (displayBackground)
                {
                    fb_putpixel(x1, y, video_bg);
                }
                break;
            }
            else if (row & 0x80)
            {
                fb_putpixel(x1, y, c);
            }
            else if (displayBackground)
            {
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

    multiboot_uint32_t color;
    unsigned i;
    struct multiboot_tag_framebuffer *tagfb = (struct multiboot_tag_framebuffer *)tag;

    video = (void *)((uint64_t)tagfb->common.framebuffer_addr + VIRT_MEM_OFFSET);
    framebuffer_type = tagfb->common.framebuffer_type;
    framebuffer_width = tagfb->common.framebuffer_width;
    framebuffer_height = tagfb->common.framebuffer_height;
    framebuffer_bpp = tagfb->common.framebuffer_bpp;
    framebuffer_pitch = tagfb->common.framebuffer_pitch;
    framebuffer_palette = tagfb->framebuffer_palette;
    framebuffer_palette_num_colors = tagfb->framebuffer_palette_num_colors;

    video_fg = framebuffer_get_color(RGB2COLOR(0xff, 0xff, 0xff));
    video_bg = framebuffer_get_color(RGB2COLOR(0, 0, 0));

    // Clear the screen
    fb_video_clearto(video_bg);

    color = framebuffer_get_color(RGB2COLOR(0, 0, 0xff));

    for (i = 0; i < framebuffer_width && i < framebuffer_height; i++)
    {
        // now using fb_putpixel
        fb_putpixel(i, i, color);
    }

    // fill a rectangle
    // top left should lie on the line and be 1/8 of the way across
    // size should be 1/2 of the screen height
    fb_video_fillrect(framebuffer_width / 8, framebuffer_width / 8, framebuffer_height / 2, framebuffer_height / 2, color);

    // Initialize font data
    header = (struct psf1_header *)&font_data_start;

    return true;
}

void scroll(uint32_t n_pixels)
{
    if (framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT)
    {
        memcpy(video, video + n_pixels * framebuffer_pitch, (framebuffer_height - n_pixels) * framebuffer_pitch);
        memset(video + (framebuffer_height - n_pixels) * framebuffer_pitch, 0, n_pixels * framebuffer_pitch);
    }
    else
    {
        uint16_t *video_memory = (uint16_t *)video;
        for (uint32_t i = 0; i < framebuffer_height - n_pixels; i++)
        {
            for (uint32_t j = 0; j < framebuffer_width; j++)
            {
                video_memory[framebuffer_width * i + j] = video_memory[framebuffer_width * (i + n_pixels) + j];
            }
        }
        for (uint32_t i = framebuffer_height - n_pixels; i < framebuffer_height; i++)
        {
            for (uint32_t j = 0; j < framebuffer_width; j++)
            {
                video_memory[framebuffer_width * i + j] = (video_bg << 12) | (video_fg << 8) | ' ';
            }
        }
    }
}

void video_putc(char c)
{
    if (framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT)
    {
        if (in_escape_sequence)
        {
            handle_escape_sequence_char(c);
            if (!in_escape_sequence)
            {
                parse_escape_sequence(escape_sequence);
                kfree(escape_sequence);
            }
            return;
        }

        if (c == '\n')
        {
            char_pos_x = 0;
            char_pos_y++;
        }
        else if (c == '\r')
        {
            char_pos_x = 0;
        }
        else if (c == '\t')
        {
            char_pos_x += 4;
        }
        else if (c == '\b')
        {
            // move cursor left, wrapping around if necessary
            // also draw a space where the cursor has moved to
            if (char_pos_x == 0)
            {
                char_pos_x = framebuffer_width / FONT_WIDTH - 1;
                char_pos_y--;
            }
            else
            {
                char_pos_x--;
            }
            // just use drawRect to draw a space, to avoid displayChar overhead and background enabling
            fb_video_fillrect(char_pos_x * FONT_WIDTH, char_pos_y * header->fontheight, FONT_WIDTH, header->fontheight, video_bg);
        }
        else if (c == '\033')
        {
            begin_escape_sequence();
        }
        else
        {
            displayChar(c, char_pos_x * FONT_WIDTH, char_pos_y * header->fontheight, video_fg, (uint8_t *)((uint64_t)&font_data_start + sizeof(struct psf1_header)));
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
            scroll(header->fontheight);
            char_pos_y--;
        }
    }
    else
    {
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

typedef struct {
    uint64_t pos; // read/write position within framebuffer
    uint64_t dependents;
} fb_data_t;

pointer_int_t fb_open(char *path, uint64_t flags, void *device_passed)
{
    UNUSED(path);
    UNUSED(device_passed);

    if (flags & O_DIRECTORY) {
        return (pointer_int_t){NULL, -ENOTDIR};
    }

    fb_data_t *data = kmalloc(sizeof(fb_data_t));
    data->pos = 0;
    data->dependents = 1;

    return (pointer_int_t){data, 0};
}

size_t fb_write(void *ptr, size_t size, size_t nmemb, void *data, void *device_passed, uint64_t flags)
{
    UNUSED(device_passed);
    UNUSED(flags);

    // calculate where to write to
    fb_data_t *fb_data = (fb_data_t *)data;
    uint64_t start_x = fb_data->pos % user_pitch;
    uint64_t start_y = fb_data->pos / user_pitch;
    uint64_t to_write = size * nmemb;

    if (fb_data->pos + to_write > framebuffer_height * user_pitch)
    {
        to_write = framebuffer_height * user_pitch - fb_data->pos;
    }

    fb_data->pos += to_write;

    // this is made unfortunately complicated due to the fact that sometimes
    // pitch != width * bpp / 8, so we have to deal with that occasionally.

    void *initial = ptr;

    // if we're not at the start of a line, write the first part of the data
    if (start_x != 0) {
        uint64_t line_remaining = user_pitch - start_x;
        if (line_remaining > to_write) {
            line_remaining = to_write;
        }
        memcpy(video + (start_y * framebuffer_pitch) + start_x, ptr, line_remaining);
        ptr += line_remaining;
        to_write -= line_remaining;
        start_y++;
    }

    // write the rest of the data
    while (to_write > 0) {
        if (to_write > user_pitch) {
            memcpy(video + (start_y * framebuffer_pitch), ptr, user_pitch);
            ptr += user_pitch;
            to_write -= user_pitch;
            start_y++;
        } else {
            memcpy(video + (start_y * framebuffer_pitch), ptr, to_write);
            ptr += to_write;
            to_write = 0;
        }
    }
    return (uint64_t)ptr - (uint64_t)initial;
}

size_t fb_read(void *ptr, size_t size, size_t nmemb, void *data, void *device_passed, uint64_t flags)
{
    UNUSED(device_passed);
    UNUSED(flags);

    // calculate where to read from
    fb_data_t *fb_data = (fb_data_t *)data;
    uint64_t start_x = fb_data->pos % user_pitch;
    uint64_t start_y = fb_data->pos / user_pitch;
    uint64_t to_read = size * nmemb;

    if (fb_data->pos + to_read > framebuffer_height * user_pitch)
    {
        to_read = framebuffer_height * user_pitch - fb_data->pos;
    }

    fb_data->pos += to_read;

    void *initial = ptr;

    // if we're not at the start of a line, read the first part of the data
    if (start_x != 0) {
        uint64_t line_remaining = user_pitch - start_x;
        if (line_remaining > to_read) {
            line_remaining = to_read;
        }
        memcpy(ptr, video + (start_y * framebuffer_pitch) + start_x, line_remaining);
        ptr += line_remaining;
        to_read -= line_remaining;
        start_y++;
    }

    // read the rest of the data
    while (to_read > 0) {
        if (to_read > user_pitch) {
            memcpy(ptr, video + (start_y * framebuffer_pitch), user_pitch);
            ptr += user_pitch;
            to_read -= user_pitch;
            start_y++;
        } else {
            memcpy(ptr, video + (start_y * framebuffer_pitch), to_read);
            ptr += to_read;
            to_read = 0;
        }
    }
    return (uint64_t)ptr - (uint64_t)initial;
}

size_t fb_file_size(void *data, void *device_passed)
{
    UNUSED(data);
    UNUSED(device_passed);

    return framebuffer_height * user_pitch;
}

off_t fb_lseek(void *data, off_t offset, int whence, device_t *device_passed)
{
    UNUSED(device_passed);

    fb_data_t *fb_data = (fb_data_t *)data;

    switch (whence)
    {
    case SEEK_SET:
        fb_data->pos = offset;
        break;
    case SEEK_CUR:
        fb_data->pos += offset;
        break;
    case SEEK_END:
        fb_data->pos = framebuffer_height * user_pitch + offset;
        break;
    }

    if (fb_data->pos > framebuffer_height * user_pitch)
    {
        fb_data->pos = framebuffer_height * user_pitch;
    }

    return fb_data->pos;
}

int fb_ioctl(void *data, unsigned long request, void *arg, void *device_passed)
{
    UNUSED(data);
    UNUSED(device_passed);

    if (request == FBIOGET_VSCREENINFO) {
        struct fb_var_screeninfo *vinfo = (struct fb_var_screeninfo *)arg;
        vinfo->xres = framebuffer_width;
        vinfo->yres = framebuffer_height;
        vinfo->xres_virtual = framebuffer_width;
        vinfo->yres_virtual = framebuffer_height;
        vinfo->bits_per_pixel = framebuffer_bpp;
        vinfo->grayscale = 0;
        return 0;
    } else if (request == 0x5401) { // TIOCGETP
        return -ENOTTY;
    }

    return -EOPNOTSUPP;
}

int fb_close(void *data, void *device_passed)
{
    UNUSED(device_passed);
    
    fb_data_t *fb_data = (fb_data_t *)data;
    if (fb_data->dependents > 1)
    {
        fb_data->dependents--;
    } else {
        kfree(data);
    }

    return 0;
}

void *fb_dup(void *data, void *device_passed)
{
    UNUSED(device_passed);

    fb_data_t *fb_data = (fb_data_t *)data;
    fb_data->dependents++;

    return data;
}

int fb_stat(void *file_entry, void *buf, void *device_passed)
{
    UNUSED(file_entry);
    UNUSED(device_passed);

    struct stat *statbuf = (struct stat *)buf;
    statbuf->st_dev = 0;
    statbuf->st_ino = 0;
    statbuf->st_mode = S_IFREG;
    statbuf->st_nlink = 1;
    statbuf->st_uid = 0;
    statbuf->st_gid = 0;
    statbuf->st_rdev = 0;
    statbuf->st_size = 0;

    return 0;
}

device_t *init_fb_device()
{
    device_t *fb_device = kmalloc(sizeof(device_t));
    strcpy(fb_device->name, "fb");
    fb_device->flags = 0;
    fb_device->data = NULL;
    fb_device->next = NULL;

    fb_device->open = (open_func_t)fb_open;
    fb_device->read = (read_func_t)fb_read;
    fb_device->close = (close_func_t)fb_close;
    fb_device->fcntl = NULL;
    fb_device->write = (write_func_t)fb_write;
    fb_device->lseek = (lseek_func_t)fb_lseek;
    fb_device->ioctl = (ioctl_func_t)fb_ioctl;
    fb_device->dup = (dup_func_t)fb_dup;
    fb_device->clone = NULL;
    fb_device->stat = (stat_func_t)fb_stat;

    fb_device->file_size = (file_size_func_t)fb_file_size;

    fb_device->type = DEVICE_TYPE_FRMEBUF;

    user_pitch = framebuffer_width * framebuffer_bpp / 8;

    return register_device(fb_device);
}