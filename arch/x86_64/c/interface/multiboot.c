#include <multiboot.h>
#include <multiboot2.h>
#include <serial.h>
#include <video.h>
#include <system.h>
#include <tables.h>
#include <memory.h>
#include <string.h>
#include <filesystem.h>
#include <ramdisk.h>

void *ramdisk_addr = 0;

void multiboot_init(kernel_info_t *info) {
    void *mbd = (void *)(uint64_t)info->multiboot_addr;

    uint32_t framebuffer_tag = 0;
    uint32_t mmap_tag_addr = 0;

    struct multiboot_tag *tag;
    for (tag = (struct multiboot_tag *)((uint64_t)mbd + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag *)((multiboot_uint8_t *)tag + ((tag->size + 7) & ~7)))
    {
        switch (tag->type)
        {
        case MULTIBOOT_TAG_TYPE_MMAP:
            mmap_tag_addr = (uint32_t)(uint64_t)tag;
            break;
        case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
            framebuffer_tag = (uint32_t)(uint64_t)tag;
            break;
        case MULTIBOOT_TAG_TYPE_MODULE:
            struct multiboot_tag_module *module = (struct multiboot_tag_module *)tag;
            if (strcmp((char *)module->cmdline, "RAMDISK") == 0)
            {
                ramdisk_addr = (void *)(uint64_t)module->mod_start;
            }
            break;
        default:
            serial_printf("Unknown multiboot tag: %d\n", tag->type);
            break;
        }
    }
    
    serial_printf("Initializing video with tag 0x%x\n", framebuffer_tag);
    video_init((struct multiboot_tag_framebuffer *)(framebuffer_tag + VIRT_MEM_OFFSET));

    serial_printf("Initializing system tables...\n");
    tables_init();

    serial_printf("Initializing memory...\n");
    memory_init(info->kheap_end, mmap_tag_addr, (uint64_t)framebuffer_tag + VIRT_MEM_OFFSET);
}