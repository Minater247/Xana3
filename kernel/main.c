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

uint32_t passed_info;

void kmain() {
    
    kernel_info_t *info = (kernel_info_t *)(uint64_t)passed_info;

    // increase RSP/RBP by VIRT_MEM_OFFSET
    uint64_t rsp, rbp;
    asm volatile ("movq %%rsp, %0;" : "=r" (rsp));
    asm volatile ("movq %%rbp, %0;" : "=r" (rbp));
    rsp += VIRT_MEM_OFFSET;
    rbp += VIRT_MEM_OFFSET;
    asm volatile ("movq %0, %%rsp" : : "r" (rsp));
    asm volatile ("movq %0, %%rbp" : : "r" (rbp));

    video_init((struct multiboot_tag_framebuffer *)((uint64_t)info->framebuffer_tag + VIRT_MEM_OFFSET));

    tables_init();

    memory_init(info->kheap_end, info->mmap_tag_addr, (uint64_t)info->framebuffer_tag + VIRT_MEM_OFFSET);

    syscall_init();

    traceback_init(info->elf_symbols_addr, info->elf_strings_addr, info->elf_symbol_count);

    printf("Hello from a 64-bit graphical kernel!!\n");

    // Adjust ramdisk accordingly
    filesystem_init(init_ramdisk_device((uint64_t)info->ramdisk_addr + VIRT_MEM_OFFSET));
    init_device_device();
    init_simple_output();
    keyboard_install();

    // test printing in ANSI color
    printf("\033[1;31mThis is red text\033[0m\n");

    printf("\n\nBefore we jump to the usermode program, roadmap:\n");
    printf("  - Multitasking\n");
    printf("  - Image drawing\n");
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
            printf("Read %d bytes\n", read);
            uint64_t entry = load_elf64(buf);
            printf("Entry point: 0x%x\n", entry);

            map_page_kmalloc(0x4000, first_free_page_addr(), false, true, current_pml4);

            // call it...!
            jump_to_usermode(entry, 0x5000);
        }
    }

    // Loop indefinitely
    while (1) {}
}

void __init(uint32_t kernel_info) {
    // We still need a jump up from identity-mapped memory
    passed_info = kernel_info;
    uint64_t kmain_addr = (uint64_t)kmain;
    asm volatile("jmp *%0" : : "r"(kmain_addr));
}