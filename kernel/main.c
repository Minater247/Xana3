#include <stdint.h>

#include <info.h>
#include <video.h>
#include <multiboot2.h>
#include <display.h>

uint32_t passed_info;

void kmain() {
    
    kernel_info_t *info = (kernel_info_t *)(uint64_t)passed_info;

    video_init((struct multiboot_tag_framebuffer *)((uint64_t)info->framebuffer_tag + 0xffffff8000000000));

    printf("Hello from a 64-bit graphical kernel!!\n");

    printf("Kheap end: 0x%lx\n", (uint64_t)info->kheap_end + 0xffffff8000000000);

    // Loop indefinitely
    while (1) {}
}

void __init(uint32_t kernel_info) {
    // We still need a jump up from identity-mapped memory
    passed_info = kernel_info;
    uint64_t kmain_addr = (uint64_t)kmain;
    asm volatile("jmp *%0" : : "r"(kmain_addr));
}