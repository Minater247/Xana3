#include <stdint.h>
#include <stddef.h>

#include <multiboot.h>
#include <multiboot2.h>
#include <display.h>
#include <status.h>
#include <errors.h>
#include <stdbool.h>
#include <serial.h>
#include <video.h>

bool has_framebuffer = false;

bool load_multiboot(uint32_t magic, void *mbd)
{
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC)
    {
        FAIL("Invalid magic number: 0x%x", magic);
        return false;
    }

    if (mbd == NULL)
    {
        FAIL("Invalid multiboot information");
        return false;
    }

    char *cmdline;
    char *bootloader_name;

    struct multiboot_tag *tag;
    for (tag = (struct multiboot_tag *)((uint32_t)mbd + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag *)((multiboot_uint8_t *)tag + ((tag->size + 7) & ~7)))
    {
        switch (tag->type)
        {
        case MULTIBOOT_TAG_TYPE_CMDLINE:
            cmdline = ((struct multiboot_tag_string *)tag)->string;
            break;
        case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
            bootloader_name = ((struct multiboot_tag_string *)tag)->string;
            break;
        case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
            //printf("Lower memory: %dKB, Upper memory: %dKB\n", ((struct multiboot_tag_basic_meminfo *)tag)->mem_lower, ((struct multiboot_tag_basic_meminfo *)tag)->mem_upper);
            break;
        case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
            has_framebuffer = true;
            video_init((struct multiboot_tag_framebuffer *)tag);
            break;
        case MULTIBOOT_TAG_TYPE_VBE:
        case MULTIBOOT_TAG_TYPE_EFI_MMAP:
        case MULTIBOOT_TAG_TYPE_EFI64:
        case MULTIBOOT_TAG_TYPE_BOOTDEV:
        case MULTIBOOT_TAG_TYPE_ELF_SECTIONS:
        case MULTIBOOT_TAG_TYPE_MMAP:
        case MULTIBOOT_TAG_TYPE_APM:
        case MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR:
        case MULTIBOOT_TAG_TYPE_ACPI_NEW:
        case MULTIBOOT_TAG_TYPE_ACPI_OLD:
            // Either we don't need this or don't yet use it
            break;
        default:
            kpanic("Unknown multiboot tag: %d\n", tag->type);
        }
    }

    if (!has_framebuffer) {
        serial_printf("No framebuffer detected\n");
    }

    return true;
}