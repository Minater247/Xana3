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

    printf("Hello from a 64-bit graphical kernel!!\n");

    printf("Kheap end: 0x%lx\n", (uint64_t)info->kheap_end + VIRT_MEM_OFFSET);

    // Adjust ramdisk accordingly
    serial_printf("Got ramdisk at 0x%lx\n", (uint64_t)info->ramdisk_addr + VIRT_MEM_OFFSET);
    filesystem_init(init_ramdisk_device((uint64_t)info->ramdisk_addr + VIRT_MEM_OFFSET));
    init_device_device();
    init_simple_output();

    int fd = fopen("/mnt/ramdisk/logo.txt", 0, 0);
    printf("FD: %d\n", fd);
    if (fd < 0) {
        printf("Failed to open file! Error code: %d / 0x%x\n", fd, fd);
    } else {
        char buf[1024];
        int read = fread(buf, 1, 1024, fd);
        if (read < 0) {
            printf("Failed to read file\n");
        } else {
            printf("Read %d bytes\n", read);
            for (int i = 0; i < read; i++) {
                printf("%c", buf[i]);
            }
        }
    }

    fd = fopen("/mnt/ramdisk/bin/XanHello.elf", 0, 0);
    printf("XANHELLO FD: %d\n", fd);
    if (fd < 0) {
        printf("Failed to open file\n");
    } else {
        size_t size = file_size_internal("/mnt/ramdisk/bin/XanHello.elf");
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