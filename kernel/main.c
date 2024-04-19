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

void __attribute__((noreturn)) kmain(kernel_info_t *info) {
    // increase RSP/RBP by VIRT_MEM_OFFSET
    uint64_t rsp, rbp;
    ASM_READ_RSP(rsp);
    ASM_READ_RBP(rbp);
    rsp += VIRT_MEM_OFFSET;
    rbp += VIRT_MEM_OFFSET;
    ASM_WRITE_RSP(rsp);
    ASM_WRITE_RBP(rbp);

    serial_printf("Initializing video...\n");
    video_init((struct multiboot_tag_framebuffer *)((uint64_t)info->framebuffer_tag + VIRT_MEM_OFFSET));

    serial_printf("Initializing system tables...\n");
    tables_init();

    serial_printf("Initializing memory...\n");
    memory_init(info->kheap_end, info->mmap_tag_addr, (uint64_t)info->framebuffer_tag + VIRT_MEM_OFFSET);

    serial_printf("Initializing syscall table...\n");
    syscall_init();

    serial_printf("Initializing traceback...\n");
    traceback_init(info->elf_symbols_addr, info->elf_strings_addr, info->elf_symbol_count);

    serial_printf("Setup complete\n");

    printf("Hello from a 64-bit graphical kernel!!\n");

    process_init();

    // Adjust ramdisk accordingly
    filesystem_init(init_ramdisk_device((uint64_t)info->ramdisk_addr + VIRT_MEM_OFFSET));
    init_device_device();
    init_simple_output();
    init_fb_device();
    keyboard_install();

    mouse_init();

    printf("\x1b[38;5;217m");
    printf("\n\nBefore we jump to the usermode program, roadmap:\n");
    printf("  - Filesystem (EXT2, ISO9660)\n");
    printf("  - ACPI AML parsing\n");
    printf("  - PCI device enumeration\n");
    printf("  - USB stack\n");
    printf("Now back to your regularly scheduled program...\n\n");

    int fd = fopen("/mnt/ramdisk/bin/Xansh.elf", 0, 0);
    if (fd < 0) {
        printf("Failed to open file\n");
    } else {
        size_t size = file_size_internal("/mnt/ramdisk/bin/Xansh.elf");
        char *buf = kmalloc(size);
        int read = fread(buf, 1, size, fd);
        if (read < 0) {
            printf("Failed to read file\n");
        } else {
            page_directory_t *pml4 = clone_page_directory(current_pml4);

            printf("Read %d bytes\n", read);
            elf_info_t info = load_elf64(buf, pml4);
            fclose(fd);

            if (info.status != 0) {
                printf("Failed to load ELF: %d\n", info.status);
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

void __attribute__((noreturn)) __init(uint32_t kernel_info) {
    UNUSED(kernel_info);
    // We still need a jump up from identity-mapped memory
    asm volatile("jmp *%0" : : "b"((uint64_t)kmain));

    while (1);
}