#ifndef _TTY_H
#define _TTY_H

#include <stdint.h>
#include <sys/types.h>

#include <device.h>

#define NCCS 32

typedef unsigned char cc_t;
typedef uint32_t speed_t;
typedef uint32_t tcflag_t;

struct termios {
    tcflag_t c_iflag;       /* input mode flags */
    tcflag_t c_oflag;       /* output mode flags */
    tcflag_t c_cflag;       /* control mode flags */
    tcflag_t c_lflag;       /* local mode flags */
    cc_t c_cc[NCCS];         /* control characters */
};

typedef struct tty {
    gid_t foreground;
    uint16_t buffer_size;
    uint16_t buffer_pos;
    char *buffer;
    uint8_t mode;
    int id;

    struct termios termios;

    struct tty *next;
} tty_t;

#define BASE_TTY_LENGTH 1024

#define TTY_MODE_RAW 0
#define TTY_MODE_COOKED 1

#define IGNBRK  (1 << 0)
#define BRKINT  (1 << 1)
#define IGNPAR  (1 << 2)
#define PARMRK  (1 << 3)
#define INPCK   (1 << 4)
#define ISTRIP  (1 << 5)
#define INLCR   (1 << 6)
#define IGNCR   (1 << 7)
#define ICRNL   (1 << 8)
#define IXON    (1 << 9)
#define IXOFF   (1 << 10)
#define IXANY   (1 << 11)
#define IMAXBEL (1 << 13)
#define IUCLC   (1 << 14)

#define OPOST   (1 << 0)
#define ONLCR   (1 << 1)
#define ONOEOT  (1 << 3)
#define OCRNL   (1 << 4)
#define ONOCR   (1 << 5)
#define ONLRET  (1 << 6)

#define NLDLY   (3 << 8)
#define NL0     (0 << 8)
#define NL1     (1 << 8)
#define TABDLY  (3 << 10 | 1 << 2)
#define TAB0    (0 << 10)
#define TAB1    (1 << 10)
#define TAB2    (2 << 10)
#define TAB3    (3 << 10)
#define CRDLY   (3 << 12)
#define CR0     (0 << 12)
#define CR1     (1 << 12)
#define CR2     (2 << 12)
#define CR3     (3 << 12)
#define FFDLY   (1 << 14)
#define FF0     (0 << 14)
#define FF1     (1 << 14)
#define BSDLY   (1 << 15)
#define BS0     (0 << 15)
#define BS1     (1 << 15)
#define VTDLY   (1 << 16)
#define VT0     (0 << 16)
#define VT1     (1 << 16)
#define OLCUC   (1 << 17)
#define OFILL   (1 << 18)
#define OFDEL   (1 << 19)

#define CIGNORE (1 << 0)
#define CSIZE   (CS5|CS6|CS7|CS8)
#define CS5     (0 << 1)
#define CS6     (1 << 8)
#define CS7     (1 << 9)
#define CS8     (CS6|CS7)
#define CSTOPB  (1 << 10)
#define CREAD   (1 << 11)
#define PARENB  (1 << 12)
#define PARODD  (1 << 13)
#define HUPCL   (1 << 14)
#define CLOCAL  (1 << 15)
#define CRTSCTS (1 << 16)
#define CRTS_IFLOW CRTSCTS
#define CCTS_OFLOW CRTSCTS
#define CDTRCTS (1 << 17)
#define MDMBUF  (1 << 20)
#define CHWFLOW (MDMBUF|CRTSCTS|CDTRCTS)

#define ECHOKE  (1 << 0)
#define ECHOE   (1 << 1)
#define ECHOK   (1 << 2)
#define ECHO    (1 << 3)
#define ECHONL  (1 << 4)
#define ECHOPRT (1 << 5)
#define ECHOCTL (1 << 6)
#define ISIG    (1 << 7)
#define ICANON  (1 << 8)
#define ALTWERASE (1 << 9)
#define IEXTEN  (1 << 10)
#define EXTPROC (1 << 11)
#define TOSTOP  (1 << 22)
#define FLUSHO  (1 << 23)
#define XCASE   (1 << 24)
#define NOKERNINFO (1 << 25)
#define PENDIN  (1 << 29)
#define NOFLSH  (1 << 31)

#define VEOF    0
#define VEOL    1
#define VEOL2   2
#define VERASE  3
#define VWERASE 4
#define VKILL   5
#define VREPRINT 6
#define VINTR   8
#define VQUIT   9
#define VSUSP   10
#define VDSUSP  11
#define VSTART  12
#define VSTOP   13
#define VLNEXT  14
#define VDISCARD 15
#define VMIN    16
#define VTIME   17
#define VSTATUS 18

#define TCSANOW 0
#define TCSADRAIN   1
#define TCSAFLUSH   2
#define TCSASOFT    0x10
#define TCIFLUSH    1
#define TCOFLUSH    2
#define TCIOFLUSH   3

#define TCOOFF  1
#define TCOON   2
#define TCIOFF  3
#define TCION   4

device_t *create_tty();
void tty_init();
void tty_addchar_internal(char c);

#endif