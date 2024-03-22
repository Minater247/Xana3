CFILES = $(wildcard *.c) $(wildcard */*.c)
ASMFILES = $(wildcard *.asm) $(wildcard */*.asm)
OBJS = $(CFILES:.c=.o) $(ASMFILES:.asm=.o)

CC = i686-elf-gcc
AS = nasm

CFLAGS = -std=gnu99 -ffreestanding -O3 -Wall -Wextra -mno-sse -mno-red-zone -m32 -mno-mmx -mno-sse2 -D_ARCH_$(ARCH) -fno-omit-frame-pointer -D__ARCH_$(ARCH)__
LDFLAGS = -T linker.ld -ffreestanding -O2 -nostdlib -lgcc
ASMFLAGS = -f elf32

CINCLUDES = -I ../../kernel/include -Iinclude

all: $(OBJS) link

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS) $(CINCLUDES)

%.o: %.asm
	$(AS) $< -o $@ $(ASMFLAGS)

link:
	$(CC) -o bootstrap.bin $(OBJS) $(LDFLAGS)

clean:
	rm -rf *.o */*.o bootstrap.bin