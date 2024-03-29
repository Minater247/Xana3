#include <stdint.h>

#include <filesystem.h>
#include <display.h>
#include <sys/errno.h>
#include <string.h>
#include <sys/types.h>
#include <memory.h>
#include <errors.h>
#include <unused.h>

dev_t next_device_id = 0;
mount_t *mounts = NULL;
file_descriptor_t *file_descriptors = NULL;

device_t *root_filesystem = NULL;

char resolution_buffer[PATH_MAX];

char pwd[PATH_MAX];

uint32_t get_path_depth(char *path) {
    uint32_t depth = 0;
    for (uint32_t i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/' && path[i + 1] != '\0' && path[i + 1] != '/') {
            depth++;
        }
    }
    return depth;
}

// Get the part of a path (absolute only!) at a certain depth
// Caller must ensure that part_num is less than the depth of the path,
// the buffer is large enough, and that the path is absolute. Behavior is
// undefined if these conditions are not met.
void get_path_part(char *path, char *part, uint32_t part_num) {
    uint32_t current_part = 0;
    uint32_t j = 0;
    for (uint32_t i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            while (path[i + 1] == '/') {
                i++;
            }
            if (current_part == part_num) {
                i += 1;
                for (j = 0; path[i + j] != '/' && path[i + j] != '\0'; j++) {
                    part[j] = path[i + j];
                }
                part[j] = '\0';
                return;
            }
            current_part++;
        } else {
            if (current_part == part_num) {
                part[i] = path[i];
            }
        }
    }   
}

int mount_at(char *path, device_t *device, char *filesystemtype, unsigned long mountflags) {
    mount_t *mount = (mount_t *)kmalloc(sizeof(mount_t));
    strcpy(mount->filesystemtype, filesystemtype);
    strcpy(mount->path, path);
    mount->mountflags = mountflags;
    mount->filesystem = device;
    mount->next = mounts;
    mounts = mount;
    return 0;
}


pointer_int_t get_path_device(char *path) {
    mount_t *current_mount = mounts;
    while (current_mount != NULL) {
        if (strncmp(current_mount->path, path, strlen(current_mount->path)) == 0) {
            if (path[strlen(current_mount->path)] == '/' || path[strlen(current_mount->path)] == '\0') {
                return (pointer_int_t){current_mount->filesystem, strlen(current_mount->path)};
            }
        }
        current_mount = current_mount->next;
    }
    return (pointer_int_t){root_filesystem, 0};
}

char *device_to_path(device_t *device) {
    mount_t *current_mount = mounts;
    while (current_mount != NULL) {
        if (current_mount->filesystem == device) {
            return current_mount->path;
        }
        current_mount = current_mount->next;
    }
    return NULL;
}

char *abs_path_cleanup(char *path) {
    // remove double slashes, if dotdot is found, remove the previous part
    // this should work in the given buffer, no external space
    uint64_t path_addr_start = (uint64_t)path;

    while (*path != '\0') {
        if (*path == '/' && *(path + 1) == '/') {
            // remove the current slash
            memmove(path, path + 1, strlen(path + 1) + 1);
        } else if (*path == '/' && *(path + 1) == '.' && *(path + 2) == '.') {
            // remove the previous part
            if (path == (char *)path_addr_start) {
                // we're at the start of the path, just remove the dotdot
                memmove(path, path + 3, strlen(path + 3) + 1);
            } else {
                // find the previous slash
                char *prev_slash = path - 1;
                while (prev_slash != (char *)path_addr_start && *prev_slash != '/') {
                    prev_slash--;
                }
                if (prev_slash == (char *)path_addr_start) {
                    // we're at the start of the path, just remove the dotdot
                    memmove(path, path + 3, strlen(path + 3) + 1);
                } else {
                    // remove the previous part
                    memmove(prev_slash, path + 3, strlen(path + 3) + 1);
                    path = prev_slash;
                }
            }
        } else {
            path++;
        }
    }

    return (char *)path_addr_start;
}

char *resolve_path(char *path) {
    if (path[0] != '/') {
        char *resolved = &resolution_buffer[0];
        // ensure the paths are short enough to fit in the buffer
        if (strlen(pwd) + strlen(path) + 2 > PATH_MAX) {
            kpanic("Path too long to resolve\n");
        }
        strcpy(resolved, pwd);
        strcat(resolved, "/");
        strcat(resolved, path);
        return resolved;
    }
    if (strlen(path) > PATH_MAX) {
        kpanic("Path too long to resolve\n");
    }
    strcpy(resolution_buffer, path);
    return resolution_buffer;
}

int fopen(char *path, int flags, mode_t mode) {
    UNUSED(mode);

    resolve_path(path);
    abs_path_cleanup(resolution_buffer);
    pointer_int_t resolution = get_path_device((char *)&resolution_buffer);
    device_t *device = resolution.pointer;

    if (device == NULL) {
        return -ENOENT;
    }

    if (device->open == NULL) {
        return -ENOTSUP;
    }

    pointer_int_t returned = device->open((char *)((uint64_t)&resolution_buffer + resolution.value), flags, device);
    if (returned.value != 0) {
        return returned.value;
    }

    file_descriptor_t *fd = (file_descriptor_t *)kmalloc(sizeof(file_descriptor_t));

    file_descriptor_t *current = file_descriptors;
    if (current == NULL) {
        fd->descriptor_id = 0;
        fd->next = NULL;
    } else {
        while (current != NULL) {
            if (current->next != NULL) {
                if (current->next->descriptor_id + 1 < current->descriptor_id) {
                    fd->descriptor_id = current->descriptor_id - 1;
                    fd->next = current->next;
                    break;
                } else {
                    current = current->next;
                }
            } else {
                fd->descriptor_id = file_descriptors->descriptor_id + 1;
                fd->next = NULL;
                break;
            }
        }
    }

    fd->flags = flags;
    fd->device = device;
    fd->data = returned.pointer;
    fd->next = file_descriptors;
    file_descriptors = fd;

    return fd->descriptor_id;
}

int fclose(int fd) {
    file_descriptor_t *current = file_descriptors;
    file_descriptor_t *prev = NULL;
    while (current != NULL) {
        if (current->descriptor_id == fd) {
            if (current->device->close != NULL) {
                current->device->close(current->data, current->device);
            }
            if (prev == NULL) {
                file_descriptors = current->next;
            } else {
                prev->next = current->next;
            }
            kfree(current);
            return 0;
        }
        prev = current;
        current = current->next;
    }
    return -EBADF;
}

size_t fread(void *ptr, size_t size, size_t nmemb, int fd) {
    file_descriptor_t *current = file_descriptors;

    while (current != NULL) {
        if (current->descriptor_id == fd) {
            if (current->device->read == NULL) {
                return -ENOTSUP;
            }
            return current->device->read(ptr, size, nmemb, current->data, current->device, current->flags);
        }
        current = current->next;
    }
    return -EBADF;
}

size_t fwrite(void *ptr, size_t size, size_t nmemb, int fd) {
    file_descriptor_t *current = file_descriptors;
    while (current != NULL) {
        if (current->descriptor_id == fd) {
            if (current->device->write == NULL) {
                return -ENOTSUP;
            }
            return current->device->write(ptr, size, nmemb, current->data, current->device, current->flags);
        }
        current = current->next;
    }
    return -EBADF;
}

size_t fgetdents64(int fd, void *ptr, size_t count) {
    file_descriptor_t *current = file_descriptors;
    while (current != NULL) {
        if (current->descriptor_id == fd) {
            if (current->device->getdents64 == NULL) {
                return -ENOTSUP;
            }
            return current->device->getdents64(ptr, count, current->data, current->device);
        }
        current = current->next;
    }
    return -EBADF;
}

size_t file_size_internal(char *path) {
    pointer_int_t resolution = get_path_device(path);
    device_t *device = resolution.pointer;

    if (device == NULL) {
        return -ENOENT;
    }

    if (device->file_size == NULL) {
        return -ENOTSUP;
    }

    return device->file_size((char *)((uint64_t)path + resolution.value), device);
}

int fcntl(int fd, int cmd, long arg) {
    file_descriptor_t *current = file_descriptors;
    while (current != NULL) {
        if (current->descriptor_id == fd) {
            if (current->device->fcntl == NULL) {
                return -ENOTSUP;
            }
            return current->device->fcntl(cmd, arg, current->data, current->device);
        }
        current = current->next;
    }
    return -EBADF;
}

int fsetpwd(char *new_pwd) {
    if (strlen(new_pwd) > PATH_MAX) {
        return -ENAMETOOLONG;
    }
    
    // relative -> absolute
    resolve_path(new_pwd);
    // if it doesn't end with a slash, add one
    if (resolution_buffer[strlen(resolution_buffer) - 1] != '/') {
        strcat(resolution_buffer, "/");

        // double check the length
        if (strlen(resolution_buffer) > PATH_MAX) {
            return -ENAMETOOLONG;
        }
    }
    abs_path_cleanup(resolution_buffer);

    strcpy(pwd, resolution_buffer);

    return 0;
}

int fgetpwd(char *buf, size_t size) {
    if (strlen(pwd) > size) {
        return -ENAMETOOLONG;
    }

    strcpy(buf, pwd);

    return 0;
}

void filesystem_init(device_t *ramdisk_device) {
    if (ramdisk_device == NULL) {
        kpanic("Ramdisk device not found, boot can't continue\n");
    }

    strcpy(pwd, "/");

    // Mount the ramdisk
    mount_at("/mnt/ramdisk", ramdisk_device, "xandisk", 0);

    root_filesystem = NULL;
    file_descriptors = NULL;
}