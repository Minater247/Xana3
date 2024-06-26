#include <stdint.h>
#include <stddef.h>

#include <ramdisk.h>
#include <string.h>
#include <serial.h>
#include <filesystem.h>

ramdisk_t boot_ramdisk;

ramdisk_file_t *ramdisk_get_path_header(char *path, ramdisk_t *ramdisk) {
    if (path[0] != '/') {
        return NULL;
    }
    ramdisk_file_t *current = ramdisk->files;
    uint32_t path_depth = get_path_depth(path);
    uint32_t i = 0;
    while (1) {
        char part[64];
        get_path_part(path, part, i);
        if (current->magic == FILE_ENTRY) {
            // check the name
            if (strcmp(part, current->file_name) == 0) {
                return current;
            } else {
                current = (ramdisk_file_t *)((uint32_t)current + sizeof(ramdisk_file_t));
            }
        } else if (current->magic == DIR_ENTRY) {
            ramdisk_file_t *entries = (ramdisk_file_t *)((uint32_t)current + sizeof(ramdisk_file_t));
            uint32_t entries_count = current->size;
            if (strcmp(part, current->file_name) == 0) {
                // if we're at the end of the path, we found the directory
                if (i == path_depth - 1) {
                    return current;
                }
                // we want to move into this directory's contents
                current = entries;
                i++;
            } else {
                // we want to skip this directory
                current = (ramdisk_file_t *)((uint32_t)current + (sizeof(ramdisk_file_t) * (entries_count + 1)));
            }
        }
        // if we're out of entries in the ramdisk, we didn't find the file
        if ((uint32_t)current >= (uint32_t)ramdisk->data) {
            break;
        }
    }
    return NULL;
}

char *ramdisk_get_path_data(char *path, ramdisk_t *ramdisk) {
    ramdisk_file_t *file = ramdisk_get_path_header(path, ramdisk);
    if (file == NULL) {
        return NULL;
    }
    if (file->magic != FILE_ENTRY) {
        return NULL;
    }
    return (char *)((uint32_t)ramdisk->data + file->file_data);
}

char *ramdisk_get_header_data(ramdisk_file_t *header, ramdisk_t *ramdisk) {
    return (char *)((uint32_t)ramdisk->data + header->file_data);
}

void ramdisk_init(uint32_t addr) {
    ramdisk_hdr_t *hdr = (ramdisk_hdr_t *)addr;
    boot_ramdisk.hdr = hdr;
    boot_ramdisk.files = (ramdisk_file_t *)((uint32_t)hdr + sizeof(ramdisk_hdr_t));
    boot_ramdisk.data = (void *)((uint32_t)hdr + hdr->hdr_size);

    serial_printf("RAMDISK: %d files, %d bytes\n", hdr->files, hdr->size);
}