ARCH ?= x86_64

CC = $(ARCH)-elf-gcc
LD = $(ARCH)-elf-ld

CFLAGS = -std=gnu99 -ffreestanding -O3 -Wall -Wextra -Iinclude -Iarch/$(ARCH)/include -Ikernel/include -mcmodel=large -mno-red-zone -ffast-math
LDFLAGS = -T arch/$(ARCH)/linker.ld
AS = nasm

CFILES = $(wildcard kernel/*.c) $(wildcard kernel/*/*.c) $(wildcard arch/$(ARCH)/c/*.c) $(wildcard arch/$(ARCH)/c/*/*.c)
ASFILES = $(wildcard arch/$(ARCH)/asm/*.asm) $(wildcard arch/$(ARCH)/asm/*/*.asm)
OBJFILES = $(CFILES:.c=.o) $(ASFILES:.asm=.o)
DBOBJFILES = $(CFILES:.c=.dbo) $(ASFILES:.asm=.dbo)

all: bootstrap kernel verify_multiboot iso

kernel: $(OBJFILES)
	$(LD) $(LDFLAGS) $(OBJFILES) -o kernel.bin

kernel_dbg: $(DBOBJFILES)
	$(LD) $(LDFLAGS) $(DBOBJFILES) -o dbg_kernel.bin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) -f elf64 $< -o $@

%.dbo: %.c
	$(CC) $(CFLAGS) -g -c $< -o $@

%.dbo: %.asm
	$(AS) -f elf64 -g $< -o $@

bootstrap:
	$(MAKE) -C arch/$(ARCH)/bootstrap
	cp arch/$(ARCH)/bootstrap/bootstrap.bin bootstrap.bin

verify_multiboot:
	@echo "Verifying multiboot2 header..."
	@grub-file --is-x86-multiboot2 bootstrap.bin

iso: bootstrap kernel ramdisk
	@rm -rf isodir
	@mkdir -p isodir/boot/grub
	@cp bootstrap.bin isodir/boot/bootstrap.bin
	@cp ramdisk.img isodir/boot/ramdisk.img
	@cp kernel.bin isodir/boot/kernel.bin
	@cp arch/$(ARCH)/grub.cfg isodir/boot/grub/grub.cfg
	@grub-mkrescue -o os.iso isodir

debug_iso: bootstrap kernel_dbg ramdisk_dbg
	@rm -rf isodir
	@mkdir -p isodir/boot/grub
	@cp bootstrap.bin isodir/boot/bootstrap.bin
	@cp ramdisk.img isodir/boot/ramdisk.img
	@cp dbg_kernel.bin isodir/boot/kernel.bin
	@cp arch/$(ARCH)/grub.cfg isodir/boot/grub/grub.cfg
	@grub-mkrescue -o debug_os.iso isodir

run: iso
	@qemu-system-x86_64 -cdrom os.iso -monitor stdio -d int,cpu_reset -accel tcg -D qemu-log.txt -cpu SandyBridge -serial file:serial.log -bios OVMF.fd

run_no_uefi: iso
	@qemu-system-x86_64 -cdrom os.iso -monitor stdio -d int,cpu_reset -accel tcg -D qemu-log.txt -cpu SandyBridge -serial file:serial.log

run_no_re:
# run, but exit on reboot
	@qemu-system-x86_64 -no-reboot -cdrom os.iso -monitor stdio -d int -accel tcg -D qemu-log.txt -cpu SandyBridge -serial file:serial.log -bios OVMF.fd

debug: debug_iso
	@qemu-system-x86_64 -cdrom debug_os.iso -monitor stdio -d int,cpu_reset -accel tcg -D qemu-log.txt -cpu SandyBridge -serial file:serial.log -s -S -bios OVMF.fd

debug_no_uefi: debug_iso
	@qemu-system-x86_64 -cdrom debug_os.iso -monitor stdio -d int,cpu_reset -accel tcg -D qemu-log.txt -cpu SandyBridge -serial file:serial.log -s -S

debug_term:
	@gdb -q -ex "target remote localhost:1234" -ex "symbol-file dbg_kernel.bin"

bochs: iso
	@"/mnt/c/Program Files/Bochs-3.0/bochs.exe" -f bochsrc.txt -q

bochs_debug: debug_iso
	@"/mnt/c/Program Files/Bochs-3.0/bochsdbg.exe" -f bochsrc_dbg.txt -q

FORCE:

ramdisk: FORCE
	@mkdir -p ramdisk/bin
	python3 buildutils/ramdisk.py ramdisk.img ramdisk/

ramdisk_dbg: FORCE
	@mkdir -p ramdisk/bin
	python3 buildutils/ramdisk.py ramdisk.img ramdisk/

clean:
	$(MAKE) -C arch/$(ARCH)/bootstrap clean
	rm -f bootstrap.bin
	rm -f os.iso
	rm -f qemu-log.txt
	rm -f serial.log
	rm -rf isodir
	rm -f debug_os.iso
	rm -f dbg_kernel.bin
	rm -f kernel.bin
	rm -f $(OBJFILES)
	rm -f $(DBOBJFILES)
	rm -f com1.out