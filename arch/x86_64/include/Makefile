CFILES = $(wildcard *.c) $(wildcard */*.c)
OFILES = $(CFILES:.c=.o)

CC=x86_64-elf-gcc
CFLAGS=-ffreestanding -Wall -Wextra -Werror -I. -I./wt-libc
LDFLAGS=-T linker.ld -ffreestanding -O2 -nostdlib -lgcc

all: $(OFILES) link

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

link:
	$(CC) -o XanHello.elf $(OFILES) $(LDFLAGS)