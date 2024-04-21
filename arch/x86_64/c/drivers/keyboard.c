// Really, really temporary keyboard driver. This is just a proof of concept and will be replaced with a proper driver later on.
// This makes a few not-so-great assumptions:
// - GRUB left the PS/2 port in a usable state (probable, but not in the spec)
// - The keyboard is in port 1
// - The keyboard is a US keyboard
// This will be remedied once ACPI -> PCI -> USB -> HID is implemented.

#include <stdint.h>
#include <stdbool.h>

#include <system.h>
#include <io.h>
#include <display.h>
#include <unused.h>
#include <device.h>
#include <filesystem.h>
#include <tables.h>
#include <string.h>
#include <sys/errno.h>

char keypress_buffer[256];
uint8_t keypress_buffer_size = 0;

bool shift = false;
bool caps = false;

device_t kbd_device;

const char kbdcodes[128] =
    {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8',    /* 9 */
        '9', '0', '-', '=', '\b',                         /* Backspace */
        '\t',                                             /* Tab */
        'q', 'w', 'e', 'r',                               /* 19 */
        't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',     /* Enter key */
        0,                                                /* 29   - Control */
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', /* 39 */
        '\'', '`', 0,                                     /* Left shift */
        '\\', 'z', 'x', 'c', 'v', 'b', 'n',               /* 49 */
        'm', ',', '.', '/', 0,                            /* Right shift */
        '*',
        0,   /* Alt */
        ' ', /* Space bar */
        0,   /* Caps lock */
        0,   /* 59 - F1 key ... > */
        0, 0, 0, 0, 0, 0, 0, 0,
        0, /* < ... F10 */
        0, /* 69 - Num lock*/
        0, /* Scroll Lock */
        0, /* Home key */
        0, /* Up Arrow */
        0, /* Page Up */
        '-',
        0, /* Left Arrow */
        0,
        0, /* Right Arrow */
        '+',
        0, /* 79 - End key*/
        0, /* Down Arrow */
        0, /* Page Down */
        0, /* Insert Key */
        0, /* Delete Key */
        0, 0, 0,
        0, /* F11 Key */
        0, /* F12 Key */
        0, /* All other keys are undefined */
};
const char kbdcodes_shifted[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*',    /* 9 */
    '(', ')', '_', '+', '\b',                         /* Backspace */
    '\t',                                             /* Tab */
    'Q', 'W', 'E', 'R',                               /* 19 */
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',     /* Enter key */
    0,                                                /* 29   - Control */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', /* 39 */
    '"', '~', 0,                                      /* Left shift */
    '|', 'Z', 'X', 'C', 'V', 'B', 'N',                /* 49 */
    'M', '<', '>', '?', 0,                            /* Right shift */
    '*',
    0,   /* Alt */
    ' ', /* Space bar */
    0,   /* Caps lock */
    0,   /* 59 - F1 key ... > */
    0, 0, 0, 0, 0, 0, 0, 0,
    0, /* < ... F10 */
    0, /* 69 - Num lock*/
    0, /* Scroll Lock */
    0, /* Home key */
    0, /* Up Arrow */
    0, /* Page Up */
    '-',
    0, /* Left Arrow */
    0,
    0, /* Right Arrow */
    '+',
    0, /* 79 - End key*/
    0, /* Down Arrow */
    0, /* Page Down */
    0, /* Insert Key */
    0, /* Delete Key */
    0, 0, 0,
    0, /* F11 Key */
    0, /* F12 Key */
    0, /* All other keys are undefined */
};

// keyboard (hardware keyboard, uninitialized PS2 at the moment)
void keyboard_interrupt_handler(regs_t *r)
{
    UNUSED(r);
    uint8_t scancode = inb(0x60);
    if (scancode & 0x80)
    {
        // key released
        scancode &= 0x7F;
        if (scancode == 0x2A || scancode == 0x36)
        {
            shift = false;
        }
    }
    else
    {
        // key pressed
        if (scancode == 0x2A || scancode == 0x36)
        {
            shift = true;
        }
        else if (scancode == 0x3A)
        {
            caps = !caps;
        }

        if (keypress_buffer_size < 255)
        {
            keypress_buffer[keypress_buffer_size] = scancode;
            keypress_buffer_size++;
        }
        else
        {
            // buffer full, shift everything down
            for (int i = 0; i < 255; i++)
            {
                keypress_buffer[i] = keypress_buffer[i + 1];
            }
            keypress_buffer[255] = scancode;
        }
    }
}

uint8_t keyboard_getcode()
{
    if (keypress_buffer_size > 0)
    {
        keypress_buffer_size--;
        char scancode = keypress_buffer[0];
        // shift everything down
        for (int i = 0; i < 255; i++)
        {
            keypress_buffer[i] = keypress_buffer[i + 1];
        }
        return scancode;
    }
    return 0;
}

char keyboard_getchar()
{
    uint8_t scancode = keyboard_getcode();
    if (scancode == 0)
    {
        return 0;
    }
    if (shift)
    {
        return kbdcodes_shifted[scancode];
    }
    if (caps)
    {
        if (kbdcodes[scancode] >= 'a' && kbdcodes[scancode] <= 'z')
        {
            return kbdcodes_shifted[scancode];
        }
    }
    return kbdcodes[scancode];
}

size_t kbd_device_read(void *ptr, size_t nmemb, size_t count, void *unused1, void *unused2, uint64_t flags)
{
    UNUSED(unused1);
    UNUSED(unused2);
    UNUSED(flags);

    size_t size = nmemb * count;

    uint32_t written = 0;
    char code;
    while ((code = keyboard_getchar()) != 0) {
        *(char *)(ptr + written) = code;
        written++;
        if (written >= size) {
            return written;
        }
    }

    // may be a better idea to make this blocking? depends. will see what comes of this
    if (written == 0) {
        return -EAGAIN;
    }

    return written;
}

pointer_int_t keyboard_open(char *path, uint64_t flags, void *device_passed) {
    UNUSED(path);
    UNUSED(flags);
    UNUSED(device_passed);

    return (pointer_int_t){NULL, 0};
}

void *kbd_device_clone(void *filedes_data, void *device_passed) {
    UNUSED(filedes_data);
    UNUSED(device_passed);

    return NULL;
}

int kbd_stat(void *file_entry, void *buf, void *device_passed) {
    UNUSED(file_entry);
    UNUSED(device_passed);

    struct stat *statbuf = (struct stat *)buf;
    statbuf->st_dev = 0;
    statbuf->st_ino = 0;
    statbuf->st_mode = S_IFCHR;
    statbuf->st_nlink = 1;
    statbuf->st_uid = 0;
    statbuf->st_gid = 0;
    statbuf->st_rdev = 0;
    statbuf->st_size = 0;

    return 0;
}

void keyboard_install()
{
    register_interrupt_handler(1, keyboard_interrupt_handler);

    //init buffer/vars
    keypress_buffer_size = 0;
    shift = false;
    caps = false;

    strcpy(kbd_device.name, "keyboard");
    kbd_device.flags = 0;
    kbd_device.data = NULL;
    kbd_device.type = DEVICE_TYPE_KYBOARD;

    kbd_device.read = (read_func_t)kbd_device_read;
    kbd_device.write = NULL;
    kbd_device.open = (open_func_t)keyboard_open;
    kbd_device.close = NULL;
    kbd_device.fcntl = NULL;
    kbd_device.file_size = NULL;
    kbd_device.lseek = NULL;
    kbd_device.clone = (clone_func_t)kbd_device_clone;
    kbd_device.stat = (stat_func_t)kbd_stat;

    register_device(&kbd_device);
}