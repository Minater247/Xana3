#include <stdint.h>
#include <stddef.h>

#include <multiboot.h>
#include <multiboot2.h>
#include <display.h>
#include <errors.h>
#include <stdbool.h>
#include <serial.h>
#include <video.h>
#include <string.h>
#include <ramdisk.h>

bool has_framebuffer = false;

extern uint8_t BOOTSTRAP_END;

uint64_t multiboot_total_memory = 0;
uint64_t multiboot_max_addr = 0;

bool load_multiboot(uint32_t magic, void *mbd)
{
    multiboot_max_addr = (uint32_t)&BOOTSTRAP_END;

    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC)
    {
        serial_printf("Invalid magic number: 0x%x", magic);
        return false;
    }

    if (mbd == NULL)
    {
        serial_printf("Invalid multiboot information");
        return false;
    }

    char *cmdline = "";
    char *bootloader_name = "Unknown";

    struct multiboot_tag *tag;
    for (tag = (struct multiboot_tag *)((uint32_t)mbd + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (struct multiboot_tag *)((multiboot_uint8_t *)tag + ((tag->size + 7) & ~7)))
    {
        switch (tag->type)
        {
        case MULTIBOOT_TAG_TYPE_CMDLINE:
            cmdline = ((struct multiboot_tag_string *)tag)->string;
            if ((uint32_t)cmdline + strlen(cmdline) > multiboot_max_addr)
            {
                multiboot_max_addr = (uint32_t)cmdline + strlen(cmdline);
            }
            break;
        case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
            bootloader_name = ((struct multiboot_tag_string *)tag)->string;
            break;
        case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
            has_framebuffer = true;
            video_init((struct multiboot_tag_framebuffer *)tag);
            // I believe this is mapped out of physical memory so we don't have to change max addr
            break;
        case MULTIBOOT_TAG_TYPE_MMAP:
        {
            // Calculate total memory
            struct multiboot_tag_mmap *mmap = (struct multiboot_tag_mmap *)tag;
            struct multiboot_mmap_entry *entry;
            for (entry = mmap->entries;
                 (multiboot_uint8_t *)entry < (multiboot_uint8_t *)mmap + mmap->size;
                 entry = (struct multiboot_mmap_entry *)((unsigned long)entry + mmap->entry_size))
            {
                multiboot_total_memory += entry->len;
            }
            if ((uint32_t)mmap + mmap->size > multiboot_max_addr)
            {
                multiboot_max_addr = (uint32_t)mmap + mmap->size;
            }
            break;
        }
        case MULTIBOOT_TAG_TYPE_MODULE:
        {
            // if the string is RAMDISK, we want to load it
            struct multiboot_tag_module *module = (struct multiboot_tag_module *)tag;
            serial_printf("Got module: %s\n", module->cmdline);
            if (strcmp((char *)module->cmdline, "RAMDISK") == 0)
            {
                ramdisk_init((uint32_t)module->mod_start);
            }
            if (module->mod_end > multiboot_max_addr)
            {
                multiboot_max_addr = module->mod_end;
            }
            break;
        }
        case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
        case MULTIBOOT_TAG_TYPE_VBE:
        case MULTIBOOT_TAG_TYPE_EFI_MMAP:
        case MULTIBOOT_TAG_TYPE_EFI64:
        case MULTIBOOT_TAG_TYPE_BOOTDEV:
        case MULTIBOOT_TAG_TYPE_ELF_SECTIONS:
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

    serial_printf("Command line: %s\n", cmdline);
    serial_printf("Bootloader: %s\n", bootloader_name);

    serial_printf("Total memory: %ldMB\n", multiboot_total_memory / 1024 / 1024);
    serial_printf("Max address: 0x%lx\n", multiboot_max_addr);

    kassert(multiboot_max_addr < 0x400000); //64-bit kernel expects to be loaded at 4MB, so we can't have other stuff above that

    return true;
}