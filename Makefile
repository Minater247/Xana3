ARCH ?= x86_64
USE_UEFI ?= 0

CC = $(ARCH)-elf-gcc
LD = $(ARCH)-elf-ld

CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Iinclude -Iarch/$(ARCH)/include
LDFLAGS = -T arch/$(ARCH)/linker.ld

CFILES = $(wildcard kernel/*.c) $(wildcard kernel/*/*.c)
OBJFILES = $(CFILES:.c=.o)

all: bootstrap kernel verify_multiboot iso

kernel: $(OBJFILES)
	$(LD) $(LDFLAGS) $(OBJFILES) -o kernel.bin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

bootstrap:
	$(MAKE) -C arch/$(ARCH)/bootstrap USE_UEFI=$(USE_UEFI)
	cp arch/$(ARCH)/bootstrap/bootstrap.bin bootstrap.bin

verify_multiboot:
	@echo "Verifying multiboot2 header..."
	@grub-file --is-x86-multiboot2 bootstrap.bin

iso: bootstrap kernel ramdisk
	@rm -rf isodir
	@mkdir -p isodir/boot/grub
	@cp bootstrap.bin isodir/boot/bootstrap.bin
	@cp ramdisk.img isodir/boot/ramdisk.img
	@cp arch/$(ARCH)/grub.cfg isodir/boot/grub/grub.cfg
	@grub-mkrescue -o os.iso isodir

run: iso
	@qemu-system-x86_64 -cdrom os.iso -monitor stdio -d int,cpu_reset -accel tcg -D qemu-log.txt -cpu SandyBridge -serial file:serial.log -bios OVMF.fd

run_no_uefi: iso
	@qemu-system-x86_64 -cdrom os.iso -monitor stdio -d int,cpu_reset -accel tcg -D qemu-log.txt -cpu SandyBridge -serial file:serial.log

run_no_re:
# run, but exit on reboot
	@qemu-system-x86_64 -no-reboot -cdrom os.iso -monitor stdio -d int -accel tcg -D qemu-log.txt -cpu SandyBridge -serial file:serial.log -bios OVMF.fd

debug: iso
	@qemu-system-x86_64 -cdrom os.iso -monitor stdio -d int,cpu_reset -accel tcg -D qemu-log.txt -cpu SandyBridge -serial file:serial.log -s -S -bios OVMF.fd

debug_term:
	@gdb -ex "target remote localhost:1234" -ex "symbol-file kernel.bin"

bochs: iso
	@"/mnt/c/Program Files/Bochs-2.7/bochsdbg.exe" -f bochsrc.txt -q

FORCE:

ramdisk: FORCE
	@mkdir -p ramdisk/bin
	@cp kernel.bin ramdisk/bin/kernel.bin
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
	rm -f com1.out