#include <stdint.h>

#include <info.h>
#include <video.h>
#include <multiboot2.h>
#include <display.h>
#include <ramdisk.h>
#include <serial.h>
#include <system.h>
#include <tables.h>
#include <memory.h>
#include <filesystem.h>
#include <ramdisk.h>
#include <elf_loader.h>
#include <tables.h>
#include <syscall.h>
#include <device.h>
#include <keyboard.h>
#include <trace.h>
#include <process.h>
#include <errors.h>
#include <unused.h>
#include <mouse.h>
#include <tty.h>
#include <pipe.h>
#include <multiboot.h>

void __attribute__((noreturn, force_align_arg_pointer)) kmain(kernel_info_t *info) {
    multiboot_init(info);

    info = (kernel_info_t *)((uint64_t)info + VIRT_MEM_OFFSET);

    serial_printf("Initializing syscall table...\n");
    syscall_init();

    traceback_init(info);

    serial_printf("Setup complete\n");

    process_init();

    // Set up filesystem and devices
    filesystem_init(init_ramdisk_device((uint64_t)ramdisk_addr + VIRT_MEM_OFFSET));
    init_device_device();
    init_fb_device();
    keyboard_install();
    mouse_init();
    tty_init();
    pipe_init();

    int fd = kfopen("/mnt/ramdisk/bin/init", 0, 0);
    if (fd < 0) {
        kpanic("Failed to load initialization program!");
    } else {
        size_t size = file_size_internal("/mnt/ramdisk/bin/init");
        char *buf = kmalloc(size);
        int read = kfread(buf, 1, size, fd);
        if (read < 0) {
            kprintf("Failed to read file\n");
        } else {
            page_directory_t *pml4 = clone_page_directory(current_pml4);

            elf_info_t info = load_elf64(buf, pml4);
            kfclose(fd);

            if (info.status != 0) {
                kprintf("Failed to load ELF: %d\n", info.status);
                kfree(buf);
                while (1);
            }

            process_t *new = create_process((void *)info.entry, 0x10000, pml4, false, info.regions);

            new->brk_start = info.max_addr;

            add_process(new);

            serial_printf("Loaded ELF, entry point: 0x%lx\n", info.entry);
        }
        kfree(buf);
    }

    enableBackground(true);

    ASM_ENABLE_INTERRUPTS;

    // Loop indefinitely
    while (1);
}