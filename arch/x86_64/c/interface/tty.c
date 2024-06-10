#include <stdint.h>
#include <sys/types.h>
#include <sys/errno.h>

#include <tty.h>
#include <device.h>
#include <errors.h>
#include <string.h>
#include <device.h>
#include <unused.h>


/*
typedef struct tty {
    gid_t foreground;
    uint64_t ttyflags;
    uint16_t buffer_size;
    uint16_t buffer_pos;
    char *buffer;
    int id;
    struct tty *next;
} tty_t;

#define BASE_TTY_LENGTH 1024
*/

tty_t *tty_list = NULL;

typedef struct {
    int ttynumber;
    tty_t *tty;
    uint64_t dependents;
} tty_open_data_t;

pointer_int_t tty_open(const char *path, uint64_t flags, void *device_passed) {
    UNUSED(flags);
    UNUSED(device_passed);

    // may or may not be leading slash
    char *part = (char *)path;
    uint32_t first_number = 0;
    while (part[first_number] != '\0' && (part[first_number] < '0' || part[first_number] > '9')) {
        first_number++;
    }

    if (part[first_number] == '\0') {
        return (pointer_int_t){NULL, -ENODEV};
    }

    int ttynumber = atoi(&part[first_number]);

    tty_t *tty = tty_list;
    while (tty != NULL) {
        if (tty->id == ttynumber) {
            tty_open_data_t *data = (tty_open_data_t *)kmalloc(sizeof(tty_open_data_t));
            data->ttynumber = ttynumber;
            data->tty = tty;
            data->dependents++;
            return (pointer_int_t){data, 0};
        }
        tty = tty->next;
    }

    return (pointer_int_t){NULL, -ENODEV};
}

//ioctl
int tty_ioctl(void *filedes_data, unsigned long request, void *arg, void *device_passed) {
    UNUSED(device_passed);

    tty_open_data_t *data = (tty_open_data_t *)filedes_data;
    tty_t *tty = data->tty;

    serial_printf("IOCTL on TTY: %x\n", request);

    switch (request) {
        case 0x5401: // TCGETS
            {
                struct termios *termios_p = (struct termios *)arg;
                if (termios_p == NULL) {
                    return -EINVAL;
                }
                memcpy(termios_p, &tty->termios, sizeof(struct termios));
            }
            break;
        case 0x5402: // TCSETS
            {
                struct termios *termios_p = (struct termios *)arg;
                memcpy(&tty->termios, termios_p, sizeof(struct termios));
                serial_printf("Set termios:\n");
                serial_printf("c_iflag: %x\n", tty->termios.c_iflag);
                serial_printf("c_oflag: %x\n", tty->termios.c_oflag);
                serial_printf("c_cflag: %x\n", tty->termios.c_cflag);
                serial_printf("c_lflag: %x\n", tty->termios.c_lflag);
                for (int i = 0; i < NCCS; i++) {
                    serial_printf("c_cc[%d]: %x\n", i, tty->termios.c_cc[i]);
                }

            }
            break;
        default:
            return -EINVAL;
    }

    return 0;
}

size_t tty_write(const void *ptr, size_t size, size_t nmemb, void *filedes_data, void *device_passed, uint64_t flags) {
    UNUSED(device_passed);
    UNUSED(filedes_data);
    UNUSED(flags);

    // we can just write to the screen with video_putc
    for (size_t i = 0; i < size * nmemb; i++) {
        char c = ((char *)ptr)[i];
        video_putc(c);
    }

    return size * nmemb;
}

size_t tty_read(void *ptr, size_t size, size_t nmemb, void *filedes_data, void *device_passed, uint64_t flags) {
    UNUSED(device_passed);
    UNUSED(flags);

    tty_t *tty = ((tty_open_data_t *)filedes_data)->tty;

    // read from the tty buffer
    if (tty->termios.c_lflag & ICANON) {
        bool have_nl = false;
        for (uint16_t i = 0; i < tty->buffer_pos; i++) {
            if (tty->buffer[i] == '\n') {
                have_nl = true;
                break;
            }
        }

        if (!have_nl) {
            // wait for a newline
            ASM_ENABLE_INTERRUPTS;
            while (true) {
                for (uint16_t i = 0; i < tty->buffer_pos; i++) {
                    if (tty->buffer[i] == '\n') {
                        have_nl = true;
                        break;
                    }
                }
                if (have_nl) {
                    break;
                }
                schedule();
            }
            ASM_DISABLE_INTERRUPTS;
        }

        // copy the buffer, up to the newline
        uint16_t i = 0;
        while (i < tty->buffer_pos && tty->buffer[i] != '\n' && i < size * nmemb) {
            ((char *)ptr)[i] = tty->buffer[i];
            i++;
        }

        // copy the newline, if we're there
        if (i < size * nmemb && tty->buffer[i] == '\n') {
            ((char *)ptr)[i] = '\n';
            i++;
        }

        // shift the buffer
        for (uint16_t j = i; j < tty->buffer_pos; j++) {
            tty->buffer[j - i] = tty->buffer[j];
        }
        tty->buffer_pos -= i;
        
        return i;
    } else {
        // copy the buffer
        for (uint16_t i = 0; i < tty->buffer_pos && i < size * nmemb; i++) {
            ((char *)ptr)[i] = tty->buffer[i];
        }

        uint16_t i = tty->buffer_pos;
        tty->buffer_pos = 0;

        return i;
    }
}


void tty_push(tty_t *tty, char c) {
    if (tty->termios.c_iflag & ICRNL) {
        if (c == '\r') {
            c = '\n';
        }
    }

    if (tty->termios.c_lflag & ECHO) {
        if (c == '\b') {
            if (tty->buffer_pos > 0) {
                video_putc('\b');
                video_putc(' ');
                video_putc('\b');
            }
        } else {
            video_putc(c);
        }
    }

    if (tty->termios.c_lflag & ICANON) {
        if (c == '\b') {
            if (tty->buffer_pos > 0) {
                tty->buffer_pos--;
            }
            return;
        }
    }

    if (tty->buffer_pos >= tty->buffer_size) {
        // need to reallocate
        char *new_buffer = (char *)kmalloc(tty->buffer_size * 2);
        memcpy(new_buffer, tty->buffer, tty->buffer_size);
        kfree(tty->buffer);
        tty->buffer = new_buffer;
        tty->buffer_size *= 2;
    }

    tty->buffer[tty->buffer_pos] = c;
    tty->buffer_pos++;
}

void tty_addchar_internal(char c) {
    // For now, just write data to tty0
    tty_t *tty = tty_list;
    while (tty != NULL) {
        if (tty->id == 0) {
            tty_push(tty, c);
            return;
        }
        tty = tty->next;
    }
}

void * tty_clone(void *filedes_data, void *device_passed) {
    UNUSED(device_passed);

    tty_open_data_t *data = (tty_open_data_t *)filedes_data;
    tty_t *tty = data->tty;

    tty_open_data_t *new_data = (tty_open_data_t *)kmalloc(sizeof(tty_open_data_t));
    new_data->ttynumber = data->ttynumber;
    new_data->tty = tty;
    new_data->dependents++;

    return new_data;
}

void *tty_dup(void *filedes_data, void *device_passed) {
    UNUSED(device_passed);

    tty_open_data_t *data = (tty_open_data_t *)filedes_data;
    data->dependents++;
    return data;
}

int tty_close(void *filedes_data, void *device_passed) {
    UNUSED(device_passed);

    tty_open_data_t *data = (tty_open_data_t *)filedes_data;
    data->dependents--;
    if (data->dependents == 0) {
        kfree(data);
    }
    return 0;
}

int tty_select(void *filedes_data, void *device_passed, int type) {
    UNUSED(device_passed);

    tty_open_data_t *data = (tty_open_data_t *)filedes_data;
    tty_t *tty = data->tty;

    if (type == SELECT_READ) {
        // If ICANO is set, we can read if there's a newline
        // Otherwise, we can read if there's anything
        if (tty->termios.c_lflag & ICANON) {
            for (uint16_t i = 0; i < tty->buffer_pos; i++) {
                if (tty->buffer[i] == '\n') {
                    return 1;
                }
            }
            return 0;
        } else {
            return tty->buffer_pos > 0;
        }
    } else if (type == SELECT_WRITE) {
        return 1;
    } else {
        return 0;
    }

}

int tty_stat(void *file_entry, void *buf, void *device_passed) {
    UNUSED(file_entry);
    UNUSED(device_passed);

    struct stat *statbuf = (struct stat *)buf;
    statbuf->st_dev = 0;
    statbuf->st_ino = 0;
    statbuf->st_mode = 0;
    statbuf->st_nlink = 1;
    statbuf->st_uid = 0;
    statbuf->st_gid = 0;
    statbuf->st_rdev = 0;
    statbuf->st_size = 0;
    statbuf->st_blksize = 64;

    return 0;
}


device_t *create_tty() {
    device_t *tty_device = (device_t *)kmalloc(sizeof(device_t));

    strcpy(tty_device->name, "tty");
    tty_device->type = DEVICE_TYPE_TTY;
    tty_device->flags = 0;
    tty_device->next = NULL;

    tty_device->data = (void *)kmalloc(sizeof(tty_t));

    tty_device->open = (open_func_t)tty_open;
    tty_device->close = (close_func_t)tty_close;
    tty_device->read = (read_func_t)tty_read;
    tty_device->write = (write_func_t)tty_write;
    tty_device->ioctl = (ioctl_func_t)tty_ioctl;
    tty_device->lseek = NULL;
    tty_device->fcntl = NULL;
    tty_device->getdents64 = NULL;
    tty_device->stat = (stat_func_t)tty_stat;
    tty_device->dup = (dup_func_t)tty_dup;
    tty_device->clone = (clone_func_t)tty_clone;
    tty_device->file_size = NULL;
    tty_device->select = (select_func_t)tty_select;



    tty_t *tty = (tty_t *)tty_device->data;
    tty->foreground = 0;
    tty->buffer = (char *)kmalloc(BASE_TTY_LENGTH);
    tty->buffer_pos = 0;
    memset(tty->buffer, 0, BASE_TTY_LENGTH);
    tty->buffer_size = BASE_TTY_LENGTH;
    tty->id = 0;

    tty->termios.c_iflag = (BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    tty->termios.c_oflag = (OPOST);
    tty->termios.c_cflag = 0;
    tty->termios.c_lflag = (ECHO | ICANON | IEXTEN | ISIG);

    return register_device(tty_device);
}

void tty_init() {
    device_t *new = create_tty();
    if (new == NULL) {
        kpanic("Failed to create tty device");
    }
    tty_list = (tty_t *)new->data;
}