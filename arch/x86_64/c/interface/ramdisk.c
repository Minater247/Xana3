#include <stdint.h>
#include <stddef.h>

#include <ramdisk.h>
#include <string.h>
#include <serial.h>
#include <filesystem.h>
#include <errors.h>
#include <system.h>
#include <sys/errno.h>
#include <memory.h>
#include <unused.h>
#include <device.h>

ramdisk_t boot_ramdisk;
device_t ramdisk_device;

ramdisk_file_t *ramdisk_get_path_header(char *path, ramdisk_t *ramdisk) {
    if (path[0] != '/') {
        serial_printf("Invalid path: %s\n", path);
        return NULL;
    }
    ramdisk_file_t *current = ramdisk->files;
    uint32_t path_depth = get_path_depth(path);
    uint32_t i = 0;
    serial_printf("Parsing %s (depth %d)\n", path, path_depth);
    while (1) {
        char part[64];
        get_path_part(path, part, i);
        if (current->magic == FILE_ENTRY) {
            // check the name
            serial_printf("Got file: %s\n", current->file_name);
            if (strcmp(part, current->file_name) == 0) {
                serial_printf("Found %s\n", current->file_name);
                return current;
            } else {
                serial_printf("Skipping %s\n", current->file_name);
                current = (ramdisk_file_t *)((uint64_t)current + sizeof(ramdisk_file_t));
            }
        } else if (current->magic == DIR_ENTRY) {
            serial_printf("Got dir: %s\n", current->file_name);
            ramdisk_file_t *entries = (ramdisk_file_t *)((uint64_t)current + sizeof(ramdisk_file_t));
            uint32_t entries_count = current->size;
            if (strcmp(part, current->file_name) == 0) {
                // if we're at the end of the path, we found the directory
                serial_printf("Comparing %d, %d\n", i, path_depth - 1);
                if (i == path_depth - 1) {
                    serial_printf("Found %s\n", current->file_name);
                    return current;
                }
                // we want to move into this directory's contents
                serial_printf("Moving into %s\n", part);
                current = entries;
                i++;
            } else {
                // we want to skip this directory
                serial_printf("Skipping %s\n", current->file_name);
                current = (ramdisk_file_t *)((uint64_t)current + (sizeof(ramdisk_file_t) * (entries_count + 1)));
            }
        }
        // if we're out of entries in the ramdisk, we didn't find the file
        if ((uint64_t)current >= (uint64_t)ramdisk->data) {
            break;
        }
    }
    serial_printf("Bad path: %s\n", path);
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
    return (char *)((uint64_t)ramdisk->data + file->file_data);
}

char *ramdisk_get_header_data(ramdisk_file_t *header, ramdisk_t *ramdisk) {
    return (char *)((uint64_t)ramdisk->data + header->file_data);
}

pointer_int_t ramdisk_open(char *path, uint64_t flags, void *device_passed) {
    UNUSED(flags);

    serial_printf("RAMDISK_OPEN: %s\n", path);

    if (path[0] != '/') {
        return (pointer_int_t){NULL, -EINVAL};
    }

    // check for write flags (XanDisk does not support writing!)
    if (flags & O_WRONLY || flags & O_RDWR) {
        return (pointer_int_t){NULL, -ENOTSUP};
    }

    device_t *device = (device_t *)device_passed;

    ramdisk_file_t *file = ramdisk_get_path_header(path, device->data);

    // TODO: check major/minor device numbers to make sure we're opening the right type of device

    serial_printf("Got file: %s\n", file->file_name);

    if (file == NULL) {
        // didn't find the file
        return (pointer_int_t){NULL, -ENOENT};
    }

    if (file->magic == FILE_ENTRY && flags & O_DIRECTORY) {
        // we found a file, but we wanted a directory
        return (pointer_int_t){NULL, -ENOTDIR};
    }

    ramdisk_file_entry_t *entry = (ramdisk_file_entry_t *)kmalloc(sizeof(ramdisk_file_entry_t));
    entry->file = file;
    entry->read_pos = 0;
    
    pointer_int_t ret = {entry, 0};
    return ret;
}

int ramdisk_close(void *file_entry, void *device_passed) {
    UNUSED(device_passed);
    ramdisk_file_entry_t *entry = (ramdisk_file_entry_t *)file_entry;
    kfree(entry);
    return 0;
}

size_t ramdisk_read(void *ptr, size_t size, size_t nmemb, void *file_entry_passed, void *device_passed, uint64_t flags) {
    UNUSED(flags);
    device_t *device = (device_t *)device_passed;
    ramdisk_t *ramdisk = (ramdisk_t *)device->data;
    ramdisk_file_entry_t *file_entry = (ramdisk_file_entry_t *)file_entry_passed;
    ramdisk_file_t *file = file_entry->file;

    if (file->magic != FILE_ENTRY) {
        return -ENOTSUP;
    }

    if (file_entry->read_pos >= file->size) {
        return 0;
    }

    uint64_t bytes_to_read = size * nmemb;
    if (file_entry->read_pos + bytes_to_read > file->size) {
        bytes_to_read = file->size - file_entry->read_pos;
    }

    memcpy(ptr, (void *)((uint64_t)ramdisk->data + file->file_data + file_entry->read_pos), bytes_to_read);

    file_entry->read_pos += bytes_to_read;

    return bytes_to_read;
}

size_t file_size(char *path, void *device_passed) {
    device_t *device = (device_t *)device_passed;
    ramdisk_t *ramdisk = (ramdisk_t *)device->data;
    ramdisk_file_t *file = ramdisk_get_path_header(path, ramdisk);
    if (file == NULL) {
        return -ENOENT;
    }
    return file->size;
}

int fnctl(int cmd, long arg, void *file_entry, void *device) {
    if (cmd == F_GETPATH) {
        // we can get the base path from the device
        char *retpath = (char *)arg;
        strcpy(retpath, device_to_path(device));
        // and then append the file name
        strcat(retpath, "/");
        ramdisk_file_entry_t *entry = (ramdisk_file_entry_t *)file_entry;
        ramdisk_file_t *file = entry->file;
        strcat(retpath, file->file_name);
        serial_printf("F_GETPATH: %s\n", retpath);
        return 0;
    }



    return -ENOTSUP;
}

device_t *init_ramdisk_device(uint64_t addr) {
    ramdisk_info_32_t *info = (ramdisk_info_32_t *)addr;

    boot_ramdisk.hdr = (ramdisk_hdr_t *)((uint64_t)info->hdr + VIRT_MEM_OFFSET);
    boot_ramdisk.files = (ramdisk_file_t *)((uint64_t)info->files + VIRT_MEM_OFFSET);
    boot_ramdisk.data = (void *)((uint64_t)info->data + VIRT_MEM_OFFSET);

    printf("Got ramdisk @ 0x%lx\n", addr);
    printf("Points to location: 0x%lx\n", boot_ramdisk.hdr);
    printf("Which contains the info:\n");
    printf("\tSize: 0x%x\n", boot_ramdisk.hdr->size);
    printf("\tFiles: %d\n", boot_ramdisk.hdr->files);
    printf("\tHeader Size: 0x%x\n", boot_ramdisk.hdr->hdr_size);
    printf("\tRoot Entries: %d\n", boot_ramdisk.hdr->root_ents);

    serial_printf("Ramdisk initialized\n");
    serial_printf("Header: 0x%lx\n", (uint64_t)boot_ramdisk.hdr);
    serial_printf("Files: 0x%lx\n", (uint64_t)boot_ramdisk.files);
    serial_printf("Data: 0x%lx\n", (uint64_t)boot_ramdisk.data);

    strcpy(ramdisk_device.name, "ramdisk");
    ramdisk_device.flags = 0;
    ramdisk_device.data = (void *)&boot_ramdisk;
    ramdisk_device.next = NULL;
    
    // we need to cast the function pointer to return void *, like a function type taking path, flags, and device_t *, and returning a void *
    ramdisk_device.open = (open_func_t)ramdisk_open;
    ramdisk_device.read = (read_func_t)ramdisk_read;
    ramdisk_device.close = (close_func_t)ramdisk_close;
    ramdisk_device.fcntl = (fcntl_func_t)fnctl;

    ramdisk_device.file_size = (file_size_func_t)file_size;

    ramdisk_device.type = DEVICE_TYPE_XANDISK;

    return register_device(&ramdisk_device);
}