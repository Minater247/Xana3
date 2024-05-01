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
device_t ramdisk_device = {0};

ramdisk_file_t *ramdisk_get_path_header(char *path, ramdisk_t *ramdisk)
{
    if (path[0] != '/')
    {
        return NULL;
    }
    ramdisk_file_t *current = ramdisk->files;
    uint32_t path_depth = get_path_depth(path);
    uint32_t i = 0;
    while (1)
    {
        char part[64];
        get_path_part(path, part, i);
        if (current->magic == FILE_ENTRY)
        {
            // check the name
            if (strcmp(part, current->file_name) == 0)
            {
                return current;
            }
            else
            {
                current = (ramdisk_file_t *)((uint64_t)current + sizeof(ramdisk_file_t));
            }
        }
        else if (current->magic == DIR_ENTRY)
        {
            ramdisk_file_t *entries = (ramdisk_file_t *)((uint64_t)current + sizeof(ramdisk_file_t));
            uint32_t entries_count = current->size;
            if (strcmp(part, current->file_name) == 0)
            {
                // if we're at the end of the path, we found the directory
                if (i == path_depth - 1)
                {
                    return current;
                }
                // we want to move into this directory's contents
                current = entries;
                i++;
            }
            else
            {
                // we want to skip this directory
                current = (ramdisk_file_t *)((uint64_t)current + (sizeof(ramdisk_file_t) * (entries_count + 1)));
            }
        }
        // if we're out of entries in the ramdisk, we didn't find the file
        if ((uint64_t)current >= (uint64_t)ramdisk->data)
        {
            break;
        }
    }
    return NULL;
}

char *ramdisk_get_path_data(char *path, ramdisk_t *ramdisk)
{
    ramdisk_file_t *file = ramdisk_get_path_header(path, ramdisk);
    if (file == NULL)
    {
        return NULL;
    }
    if (file->magic != FILE_ENTRY)
    {
        return NULL;
    }
    return (char *)((uint64_t)ramdisk->data + file->file_data);
}

char *ramdisk_get_header_data(ramdisk_file_t *header, ramdisk_t *ramdisk)
{
    return (char *)((uint64_t)ramdisk->data + header->file_data);
}

pointer_int_t ramdisk_open(char *path, uint64_t flags, void *device_passed)
{
    UNUSED(flags);

    // check for write flags (XanDisk does not support writing!)
    if (flags & O_WRONLY || flags & O_RDWR)
    {
        serial_printf("Write flags not supported\n");
        return (pointer_int_t){NULL, -EOPNOTSUPP};
    }

    if (strncmp(path, "/", 2) == 0 || *path == '\0')
    {
        // root directory
        ramdisk_file_entry_t *entry = (ramdisk_file_entry_t *)kmalloc(sizeof(ramdisk_file_entry_t));
        entry->file = NULL;
        entry->read_pos = 0;
        entry->dependents = 1;
        return (pointer_int_t){entry, 0};
    }

    // anything other than root must be absolute
    if (path[0] != '/')
    {
        return (pointer_int_t){NULL, -EINVAL};
    }

    device_t *device = (device_t *)device_passed;

    ramdisk_file_t *file = ramdisk_get_path_header(path, device->data);

    // TODO: check major/minor device numbers to make sure we're opening the right type of device

    if (file == NULL)
    {
        // didn't find the file
        return (pointer_int_t){NULL, -ENOENT};
    }

    if (file == NULL)
    {
        // didn't find the file
        return (pointer_int_t){NULL, -ENOENT};
    }

    if (file->magic == FILE_ENTRY && flags & O_DIRECTORY)
    {
        // we found a file, but we wanted a directory
        return (pointer_int_t){NULL, -ENOTDIR};
    }

    ramdisk_file_entry_t *entry = (ramdisk_file_entry_t *)kmalloc(sizeof(ramdisk_file_entry_t));
    entry->file = file;
    entry->read_pos = 0;
    entry->dependents = 1;

    pointer_int_t ret = {entry, 0};
    return ret;
}

int ramdisk_close(void *file_entry, void *device_passed)
{
    UNUSED(device_passed);
    ramdisk_file_entry_t *entry = (ramdisk_file_entry_t *)file_entry;
    if (entry->dependents > 1)
    {
        entry->dependents--;
    } else {
        kfree(entry);
    }
    return 0;
}

size_t ramdisk_read(void *ptr, size_t size, size_t nmemb, void *file_entry_passed, void *device_passed, uint64_t flags)
{
    UNUSED(flags);
    device_t *device = (device_t *)device_passed;
    ramdisk_t *ramdisk = (ramdisk_t *)device->data;
    ramdisk_file_entry_t *file_entry = (ramdisk_file_entry_t *)file_entry_passed;
    ramdisk_file_t *file = file_entry->file;

    if (file->magic != FILE_ENTRY)
    {
        return -EOPNOTSUPP;
    }

    if (file_entry->read_pos >= file->size)
    {
        return 0;
    }

    uint64_t bytes_to_read = size * nmemb;
    if (file_entry->read_pos + bytes_to_read > file->size)
    {
        bytes_to_read = file->size - file_entry->read_pos;
    }

    memcpy(ptr, (void *)((uint64_t)ramdisk->data + file->file_data + file_entry->read_pos), bytes_to_read);

    file_entry->read_pos += bytes_to_read;

    return bytes_to_read;
}

size_t ramdisk_read_dirents64(void *ptr, size_t count, void *file_entry_passed, void *device_passed) {
    ramdisk_file_entry_t *file_entry = (ramdisk_file_entry_t *)file_entry_passed;

    device_t *device = (device_t *)device_passed;

    ramdisk_file_t *current = file_entry->file == NULL ? ((ramdisk_t *)device->data)->files : (ramdisk_file_t *)((uint64_t)file_entry->file + sizeof(ramdisk_file_t));

    uint32_t max_ents = file_entry->file == NULL ? ((ramdisk_t *)device->data)->hdr->root_ents : file_entry->file->size;
    if (file_entry->read_pos >= max_ents) {
        return 0;
    }

    struct dirent64 *dirent = (struct dirent64 *)ptr;
    uint32_t i = 0;
    // skip past the already read entries
    for (i = 0; i < file_entry->read_pos; i++) {
        if (current->magic == FILE_ENTRY) {
            current = (ramdisk_file_t *)((uint64_t)current + sizeof(ramdisk_file_t));
        } else if (current->magic == DIR_ENTRY) {
            uint32_t entries_count = current->size;
            current = (ramdisk_file_t *)((uint64_t)current + (sizeof(ramdisk_file_t) * (entries_count + 1)));
        }
    }

    uint32_t entries_read = 0;
    size_t bytes_read = 0;
    while (1) {
        if (sizeof(struct dirent64) + strlen(current->file_name) + 1 > count) {
            break;
        }

        if (file_entry->read_pos >= max_ents) {
            break;
        }

        dirent->d_ino = 0;
        dirent->d_off = 0;
        dirent->d_reclen = sizeof(struct dirent64) + strlen(current->file_name) + 1;
        strcpy(dirent->d_name, current->file_name);

        bytes_read += dirent->d_reclen;

        entries_read++;
        dirent = (struct dirent64 *)((uint64_t)dirent + dirent->d_reclen);
        count -= sizeof(struct dirent64);

        if (current->magic == FILE_ENTRY) {
            current = (ramdisk_file_t *)((uint64_t)current + sizeof(ramdisk_file_t));
        } else if (current->magic == DIR_ENTRY) {
            uint32_t entries_count = current->size;
            current = (ramdisk_file_t *)((uint64_t)current + (sizeof(ramdisk_file_t) * (entries_count + 1)));
        }

        file_entry->read_pos++;
    }

    return entries_read * sizeof(struct dirent64);
}

size_t file_size(char *path, void *device_passed)
{
    device_t *device = (device_t *)device_passed;
    ramdisk_t *ramdisk = (ramdisk_t *)device->data;
    ramdisk_file_t *file = ramdisk_get_path_header(path, ramdisk);
    if (file == NULL)
    {
        return -ENOENT;
    }
    return file->size;
}

int ramdisk_fnctl(int cmd, long arg, void *file_entry, void *device)
{
    if (cmd == F_GETPATH)
    {
        // we can get the base path from the device
        char *retpath = (char *)arg;
        strcpy(retpath, device_to_path(device));
        // and then append the file name
        strcat(retpath, "/");
        ramdisk_file_entry_t *entry = (ramdisk_file_entry_t *)file_entry;
        // NULL means root directory, which works out since it already has the trailing slash
        ramdisk_file_t *file = entry->file;
        if (file) {
            strcat(retpath, file->file_name);
        }
        return 0;
    }

    return -EOPNOTSUPP;
}

int ramdisk_stat(void *file_entry, void *buf, void *device_passed)
{
    UNUSED(device_passed);
    ramdisk_file_entry_t *entry = (ramdisk_file_entry_t *)file_entry;
    if (entry->file) {
        ramdisk_file_t *file = entry->file;

        struct stat *statbuf = (struct stat *)buf;
        statbuf->st_dev = 0;
        statbuf->st_ino = 0;
        statbuf->st_mode = file->magic == FILE_ENTRY ? S_IFREG : S_IFDIR;
        statbuf->st_nlink = 1;
        statbuf->st_uid = 0;
        statbuf->st_gid = 0;
        statbuf->st_rdev = 0;
        statbuf->st_size = file->size;
    } else {
        // root directory
        struct stat *statbuf = (struct stat *)buf;
        statbuf->st_dev = 0;
        statbuf->st_ino = 0;
        statbuf->st_mode = S_IFDIR;
        statbuf->st_nlink = 1;
        statbuf->st_uid = 0;
        statbuf->st_gid = 0;
        statbuf->st_rdev = 0;
        statbuf->st_size = boot_ramdisk.hdr->root_ents;
    }

    return 0;
}

void *ramdisk_dup(void *file_entry, void *device_passed)
{
    UNUSED(device_passed);
    ramdisk_file_entry_t *entry = (ramdisk_file_entry_t *)file_entry;
    entry->dependents++;
    return entry;
}

void *ramdisk_clone(void *file_entry, void *device_passed)
{
    UNUSED(device_passed);

    ramdisk_file_entry_t *new = (ramdisk_file_entry_t *)kmalloc(sizeof(ramdisk_file_entry_t));
    memcpy(new, file_entry, sizeof(ramdisk_file_entry_t));

    return new;
}

off_t ramdisk_lseek(void *file_entry, off_t offset, int whence, void *device_passed)
{
    UNUSED(device_passed);
    ramdisk_file_entry_t *entry = (ramdisk_file_entry_t *)file_entry;
    ramdisk_file_t *file = entry->file;

    switch (whence)
    {
    case SEEK_SET:
        entry->read_pos = offset;
        break;
    case SEEK_CUR:
        entry->read_pos += offset;
        break;
    case SEEK_END:
        entry->read_pos = file->size + offset;
        break;
    default:
        return -EINVAL;
    }

    return entry->read_pos;
}

device_t *init_ramdisk_device(uint64_t addr)
{
    ramdisk_info_32_t *info = (ramdisk_info_32_t *)addr;

    boot_ramdisk.hdr = (ramdisk_hdr_t *)((uint64_t)info->hdr + VIRT_MEM_OFFSET);
    boot_ramdisk.files = (ramdisk_file_t *)((uint64_t)info->files + VIRT_MEM_OFFSET);
    boot_ramdisk.data = (void *)((uint64_t)info->data + VIRT_MEM_OFFSET);

    strcpy(ramdisk_device.name, "ramdisk");
    ramdisk_device.flags = 0;
    ramdisk_device.data = (void *)&boot_ramdisk;
    ramdisk_device.next = NULL;

    // we need to cast the function pointer to return void *, like a function type taking path, flags, and device_t *, and returning a void *
    ramdisk_device.open = (open_func_t)ramdisk_open;
    ramdisk_device.read = (read_func_t)ramdisk_read;
    ramdisk_device.close = (close_func_t)ramdisk_close;
    ramdisk_device.fcntl = (fcntl_func_t)ramdisk_fnctl;
    ramdisk_device.getdents64 = (getdents64_func_t)ramdisk_read_dirents64;
    ramdisk_device.lseek = (lseek_func_t)ramdisk_lseek;
    ramdisk_device.stat = ramdisk_stat;
    ramdisk_device.dup = ramdisk_dup;
    ramdisk_device.clone = ramdisk_clone;

    ramdisk_device.file_size = (file_size_func_t)file_size;

    ramdisk_device.type = DEVICE_TYPE_XANDISK;

    return register_device(&ramdisk_device);
}