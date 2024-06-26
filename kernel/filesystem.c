#include <stdint.h>

#include <filesystem.h>
#include <display.h>
#include <sys/errno.h>
#include <string.h>
#include <sys/types.h>
#include <memory.h>
#include <errors.h>
#include <unused.h>
#include <string.h>
#include <system.h>

dev_t next_device_id = 0;
mount_t *mounts = NULL;

device_t *root_filesystem = NULL;

char resolution_buffer[PATH_MAX];

/**
 * Get the depth of a path (for example, /a/b/c has a depth of 3).
 * Should be used for absolute paths only.
 * 
 * @param path The path to get the depth of
 * 
 * @return The depth of the path (0 if the path is empty, 1 if it's just "/" etc.)
 */
uint32_t get_path_depth(char *path) {
    uint32_t depth = 0;
    for (uint32_t i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/' && path[i + 1] != '\0' && path[i + 1] != '/') {
            depth++;
        }
    }
    return depth;
}

/**
 * Get a part of a path at a certain depth. Caller must ensure part_num < get_path_depth(path),
 * the buffer is large enough, and the path is absolute. Behavior is undefined if these conditions
 * are not met.
 * 
 * @param path The path to get the part from
 * @param part The buffer to write the part to
 * @param part_num The depth to get the part at
 */
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

/**
 * Mount a filesystem device at a certain path.
 * 
 * @param path The path to mount the filesystem at
 * @param device The device to mount
 * @param filesystemtype The type of the filesystem
 * @param mountflags Flags to use when mounting
 * 
 * @return 0 if successful, -1 if not
*/
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

/**
 * Get the device mounted at a certain path.
 * 
 * @param path The path to get the device for
 * 
 * @return .pointer: The device mounted at the path
 *         .value: The length of the path section that the device is mounted at
 */
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

/**
 * Get the path that a device is mounted at.
 * 
 * @param device The device to get the path for
 * 
 * @return The path that the device is mounted at
 */
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

/**
 * Clean up an absolute path.
 * 
 * @param path The path to clean up
 * 
 * @return The cleaned up path
*/
char *abs_path_cleanup(char *path) {
    // remove double slashes, if dotdot is found, remove the previous part
    // this should work in the given buffer, no external space
    uint64_t path_addr_start = (uint64_t)path;

    while (*path != '\0') {
        if (*path == '/' && *(path + 1) == '/') {
            // remove the current slash
            memmove(path, path + 1, strlen(path + 1) + 1);
        } else if (*path == '/' && *(path + 1) == '.' && *(path + 2) == '.' && (*(path + 3) == '/' || *(path + 3) == '\0')) {
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
        } else if (*path == '/' && *(path + 1) == '.' && (*(path + 2) == '/' || *(path + 2) == '\0')) {
            // remove the dot
            memmove(path, path + 2, strlen(path + 2) + 1);
        } else {
            path++;
        }
    }

    return (char *)path_addr_start;
}

/**
 * Resolve a path to an absolute path.
 * 
 * @param path The path to resolve
 * 
 * @return The resolved path
*/
char *resolve_path(char *path) {
    if (path[0] != '/') {
        char *resolved = &resolution_buffer[0];
        // ensure the paths are short enough to fit in the buffer
        if (strlen((const char *)current_process->pwd) + strlen(path) + 2 > PATH_MAX) {
            kpanic("Path too long to resolve\n");
        }
        strcpy(resolved, (const char *)current_process->pwd);
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

void serial_dump_descriptors(file_descriptor_t *currfd) {
    while (currfd) {
        serial_printf("---------------");
        serial_printf("Id: %d\n", currfd->descriptor_id);
        serial_printf("Dv: 0x%lx\n", currfd->device);
        serial_printf("Nx: 0x%lx\n", currfd->next);
        currfd = currfd->next;
    }
    serial_printf("---------------");
}

int add_descriptor(file_descriptor_t *fd) {
    file_descriptor_t *current = current_process->file_descriptors;
    if (current == NULL) {
        fd->descriptor_id = 0;
        fd->next = NULL;
        current_process->file_descriptors = fd;
    } else {
        file_descriptor_t *prev = NULL;
        while (current != NULL) {
            if (prev != NULL && current->descriptor_id > prev->descriptor_id + 1) {
                fd->descriptor_id = prev->descriptor_id + 1;
                fd->next = current;
                prev->next = fd;
                break;
            }
            prev = current;
            current = current->next;
        }
        if (current == NULL) {
            fd->descriptor_id = prev->descriptor_id + 1;
            fd->next = NULL;
            prev->next = fd;
        }
    }
    return fd->descriptor_id;
}

/**
 * Open a file.
 * 
 * @param path The path to the file to open
 * @param flags The flags to open the file with
 * @param mode The mode to open the file with
 * 
 * @return The file descriptor of the opened file
*/
int kfopen(char *path, int flags, mode_t mode) {
    UNUSED(mode);

    resolve_path(path);
    abs_path_cleanup(resolution_buffer);
    pointer_int_t resolution = get_path_device((char *)&resolution_buffer);
    device_t *device = resolution.pointer;

    if (device == NULL) {
        return -ENOENT;
    }

    if (device->open == NULL) {
        return -EOPNOTSUPP;
    }

    pointer_int_t returned = device->open((char *)((uint64_t)&resolution_buffer + resolution.value), flags, device);
    if (returned.value != 0) {
        serial_printf("Error opening file: %d\n", returned.value);
        serial_printf("Filename of error: %s\n", (char *)((uint64_t)&resolution_buffer));
        return returned.value;
    }

    file_descriptor_t *fd = (file_descriptor_t *)kmalloc(sizeof(file_descriptor_t));
    fd->flags = flags;
    fd->device = device;
    fd->data = returned.pointer;

    return add_descriptor(fd);
}

/**
 * Close a file.
 * 
 * @param fd The file descriptor to close
 * 
 * @return 0 if successful, -1 if not
*/
int kfclose(int fd) {
    file_descriptor_t *current = current_process->file_descriptors;
    file_descriptor_t *prev = NULL;

    while (current != NULL) {
        if (current->descriptor_id == fd) {
            if (current->device->close != NULL) {
                current->device->close(current->data, current->device);
            }
            if (prev == NULL) {
                current_process->file_descriptors = current->next;
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

/**
 * Read from a file.
 * 
 * @param ptr The buffer to read into
 * @param size The size of each element to read
 * @param nmemb The number of elements to read
 * @param fd The file descriptor to read from
 * 
 * @return The number of elements read
*/
size_t kfread(void *ptr, size_t size, size_t nmemb, int fd) {
    file_descriptor_t *current = current_process->file_descriptors;

    while (current != NULL) {
        if (current->descriptor_id == fd) {
            if (current->device->read == NULL) {
                return -EOPNOTSUPP;
            }
            return current->device->read(ptr, size, nmemb, current->data, current->device, current->flags);
        }
        current = current->next;
    }
    return -EBADF;
}

/**
 * Write to a file.
 * 
 * @param ptr The buffer to write from
 * @param size The size of each element to write
 * @param nmemb The number of elements to write
 * @param fd The file descriptor to write to
 * 
 * @return The number of elements written
*/
size_t kfwrite(void *ptr, size_t size, size_t nmemb, int fd) {
    file_descriptor_t *current = current_process->file_descriptors;
    while (current != NULL) {
        if (current->descriptor_id == fd) {
            if (current->device->write == NULL) {
                return -EOPNOTSUPP;
            }
            return current->device->write(ptr, size, nmemb, current->data, current->device, current->flags);
        }
        current = current->next;
    }
    return -EBADF;
}

/**
 * Get the directory entries of a directory.
 * 
 * @param fd The file descriptor of the directory
 * @param ptr The buffer to write the entries to
 * @param count The number of bytes available in the buffer
 * 
 * @return The number of bytes written
 *        -EOPNOTSUPP if the device doesn't support this operation
 *        -EBADF if the file descriptor is invalid
 *        -ENAMETOOLONG if the path is too long
 */
size_t kfgetdents64(int fd, void *ptr, size_t count) {
    file_descriptor_t *current = current_process->file_descriptors;
    while (current != NULL) {
        if (current->descriptor_id == fd) {
            if (current->device->getdents64 == NULL) {
                return -EOPNOTSUPP;
            }
            return current->device->getdents64(ptr, count, current->data, current->device);
        }
        current = current->next;
    }
    return -EBADF;
}

/**
 * Duplicate a file descriptor. Flags are not duplicated, notably CLOEXEC is dropped.
 * 
 * @param fd The file descriptor to duplicate
 * 
 * @return The new file descriptor
*/
int kdup(int oldfd) {
    file_descriptor_t *current = current_process->file_descriptors;
    while (current != NULL) {
        if (current->descriptor_id == oldfd) {
            file_descriptor_t *fd = (file_descriptor_t *)kmalloc(sizeof(file_descriptor_t));
            if (fd->device->dup == NULL) {
                return -EOPNOTSUPP;
            }
            fd->flags = current->flags & ~O_CLOEXEC;
            fd->device = current->device;
            fd->data = current->device->dup(current->data, current->device);
            if (fd->data == NULL) {
                kfree(fd);
                return -EOPNOTSUPP;
            }

            return add_descriptor(fd);
        }
        current = current->next;
    }
    return -EBADF;
}

/**
 * Duplicate a file descriptor with a new file descriptor number.
 * Silently (and without reporting any error) closes newfd if it was already open.
 * 
 * @param oldfd The file descriptor to duplicate
 * @param newfd The new file descriptor number
 * 
 * @return The new file descriptor
*/
int kdup2(int oldfd, int newfd) {
    file_descriptor_t *current = current_process->file_descriptors;
    while (current != NULL) {
        if (current->descriptor_id == oldfd) {
            file_descriptor_t *fd = (file_descriptor_t *)kmalloc(sizeof(file_descriptor_t));

            if (current->device->dup == NULL) {
                return -EOPNOTSUPP;
            }
            fd->flags = current->flags & ~O_CLOEXEC;
            fd->device = current->device;
            fd->data = current->device->dup(current->data, current->device);
            fd->descriptor_id = newfd;

            // Check if newfd is already open
            file_descriptor_t *newfd_current = current_process->file_descriptors;
            file_descriptor_t *newfd_prev = NULL;
            while (newfd_current != NULL) {
                if (newfd_current->descriptor_id == newfd) {
                    kfclose(newfd);
                    if (newfd_prev == NULL) {
                        current_process->file_descriptors = fd;
                        fd->next = newfd_current->next;
                    } else {
                        newfd_prev->next = fd;
                        fd->next = newfd_current->next;
                    }
                    break;
                } else if (newfd_current->descriptor_id > newfd) {
                    if (newfd_prev) {
                        newfd_prev->next = fd;
                        fd->next = newfd_current;
                    } else {
                        current_process->file_descriptors = fd;
                        fd->next = newfd_current;
                    }
                    break;
                } else if (newfd_current->next == NULL) {
                    newfd_current->next = fd;
                    fd->next = NULL;
                    break;
                }
                newfd_prev = newfd_current;
                newfd_current = newfd_current->next;
            }

            return fd->descriptor_id;
        }
        current = current->next;
    }
    return -EBADF;
}

/**
 * Seek to a certain position in a file.
 * 
 * @param fd The file descriptor to seek in
 * @param offset The offset to seek to
 * @param whence The position to seek from
 * 
 * @return The new position in the file
 *        -EOPNOTSUPP if the device doesn't support this operation
 *        -EBADF if the file descriptor is invalid
 */
off_t kflseek(int fd, off_t offset, int whence) {
    file_descriptor_t *current = current_process->file_descriptors;
    while (current != NULL) {
        if (current->descriptor_id == fd) {
            if (current->device->lseek == NULL) {
                return -EOPNOTSUPP;
            }
            return current->device->lseek(current->data, offset, whence, current->device);
        }
        current = current->next;
    }
    return -EBADF;
}

/**
 * Read the stat of a file.
 * 
 * @param fd The file descriptor to get the stat of
 * @param buf The buffer to write the stat to
 * 
 * @return 0 if successful
 *        -EOPNOTSUPP if the device doesn't support this operation
 *        -EBADF if the file descriptor is invalid
 */
int kfstat(int fd, struct stat *buf) {
    file_descriptor_t *current = current_process->file_descriptors;
    while (current != NULL) {
        if (current->descriptor_id == fd) {
            if (current->device->stat == NULL) {
                return -EOPNOTSUPP;
            }
            return current->device->stat(current->data, buf, current->device);
        }
        current = current->next;
    }
    return -EBADF;
}

/**
 * Read the stat of a file by path.
 * 
 * @param path The path to get the stat of
 * @param buf The buffer to write the stat to
 * 
 * @return 0 if successful
 *       -ENOENT if the file doesn't exist
 *      -EOPNOTSUPP if the device doesn't support this operation
*/
int kstat(char *path, struct stat *buf) {
    int fd = kfopen(path, 0, 0);
    if (fd < 0) {
        return fd;
    }

    int ret = kfstat(fd, buf);

    kfclose(fd);

    return ret;
}

/**
 * Clone file descriptors for the opening of a new process.
 * 
 * @param descriptors The file descriptors to clone
 * 
 * @return The cloned file descriptors
*/
file_descriptor_t *clone_file_descriptors(file_descriptor_t *descriptors) {
    file_descriptor_t *current = descriptors;
    file_descriptor_t *new = NULL;
    file_descriptor_t *prev = NULL;
    while (current != NULL) {
        file_descriptor_t *fd = (file_descriptor_t *)kmalloc(sizeof(file_descriptor_t));
        fd->descriptor_id = current->descriptor_id;
        fd->flags = current->flags;
        fd->device = current->device;
        fd->data = current->device->clone(current->data, current->device);
        fd->next = NULL;
        if (prev == NULL) {
            new = fd;
        } else {
            prev->next = fd;
        }
        prev = fd;
        current = current->next;
    }
    return new;
}

/**
 * Get the size of a file.
 * 
 * @param path The path to the file to get the size of
 * 
 * @return The size of the file
 *        -ENOENT if the file doesn't exist
 *        -EOPNOTSUPP if the device doesn't support this operation
 */
size_t file_size_internal(char *path) {

    resolve_path(path);
    abs_path_cleanup(resolution_buffer);
    pointer_int_t resolution = get_path_device((char *)&resolution_buffer);
    device_t *device = resolution.pointer;

    if (device == NULL) {
        return -ENOENT;
    }

    if (device->file_size == NULL) {
        return -EOPNOTSUPP;
    }

    return device->file_size((char *)((uint64_t)&resolution_buffer + resolution.value), device);
}

/**
 * File control parameters for a file.
 * 
 * @param fd The file descriptor to control
 * @param cmd The command to run
 * @param arg The argument to the command
 * 
 * @return 0 if successful
 *       -EOPNOTSUPP if the device doesn't support this operation
 *       -EBADF if the file descriptor is invalid
*/
int kfcntl(int fd, int cmd, long arg) {
    file_descriptor_t *current = current_process->file_descriptors;
    while (current != NULL) {
        if (current->descriptor_id == fd) {
            if (current->device->fcntl == NULL) {
                return -EOPNOTSUPP;
            }
            return current->device->fcntl(cmd, arg, current->data, current->device);
        }
        current = current->next;
    }
    return -EBADF;
}

/**
 * Change the current working directory.
 * 
 * @param new_pwd The new working directory
 * 
 * @return 0 if successful
 *       -ENAMETOOLONG if the path is too long
*/
int kfsetpwd(char *new_pwd) {
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

    strcpy((char *)current_process->pwd, resolution_buffer);

    return 0;
}

/**
 * Get the current working directory.
 * 
 * @param buf The buffer to write the working directory to
 * @param size The size of the buffer
 * 
 * @return The working directory, or NULL if an error occurred
*/
char *kfgetpwd(char *buf, size_t size) {
    if (strlen((const char *)current_process->pwd) > size) {
        serial_printf("getpwd: path too long\n");
        return NULL;
    }

    // ensure buf is non-null
    if (buf == NULL) {
        return NULL;
    }

    strcpy(buf, (const char *)current_process->pwd);

    serial_printf("getpwd: %s\n", buf);
    serial_printf("Returning 0x%lx\n", (uint64_t)buf);

    return buf;
}

/**
 * Check whether a set of file descriptors is ready for reading, writing, or has an error.
 * 
 * @param nfds The number of file descriptors
 * @param readfds The file descriptors to check for reading
 * @param writefds The file descriptors to check for writing
 * @param errorfds The file descriptors to check for errors
 * @param timeout The timeout for the select
 * 
 * @return The number of file descriptors ready
*/
int kselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *timeout) {
    UNUSED(timeout);

    int ready = 0;
    
    // Wait for one to be ready (ignore timeout for the moment)
    // Also note file descriptors are in order, but there may be gaps if some are closed
    ASM_ENABLE_INTERRUPTS;
    while (ready == 0) {
        file_descriptor_t *current = current_process->file_descriptors;
        for (int i = 0; i < nfds; i++) {
            if (!current) {
                break;
            }

            if (!current->device->select) {
                current = current->next;
                continue;
            }

            if (current->descriptor_id == i) {
                if (readfds && (readfds->fds_bits[i / NFDBITS] & (1 << (i % NFDBITS)))) {
                    int status = current->device->select(current->data, current->device, SELECT_READ);

                    if (status < 0) {
                        serial_printf("Error in select (device name: %s): %d\n", current->device ? current->device->name : "unknown", status);
                        return status;
                    } else if (status == 1) {
                        ready++;
                    }
                }
                if (writefds && (writefds->fds_bits[i / NFDBITS] & (1 << (i % NFDBITS)))) {
                    int status = current->device->select(current->data, current->device, SELECT_WRITE);

                    if (status < 0) {
                        serial_printf("Error in select (device name: %s): %d\n", current->device ? current->device->name : "unknown", status);
                        return status;
                    } else if (status == 1) {
                        ready++;
                    }
                }
                if (errorfds && (errorfds->fds_bits[i / NFDBITS] & (1 << (i % NFDBITS)))) {
                    int status = current->device->select(current->data, current->device, SELECT_ERROR);

                    if (status < 0) {
                        serial_printf("Error in select (device name: %s): %d\n", current->device ? current->device->name : "unknown", status);
                        return status;
                    } else if (status == 1) {
                        ready++;
                    }
                }
            }
            current = current->next;
        }
        if (ready == 0) {
            schedule();
        }
    }
    ASM_DISABLE_INTERRUPTS;

    // kfree(fds);

    return ready;
}

/**
 * IOCTL for a file descriptor.
 * 
 * @param fd The file descriptor to run the ioctl on
 * @param request The request to run
 * @param arg The argument to the request
 * 
 * @return 0 if successful
 *      -EOPNOTSUPP if the device doesn't support this operation
 *      -EBADF if the file descriptor is invalid
 *      -EINVAL if the request is invalid
 */
int kioctl(int fd, unsigned long request, void *arg) {
    file_descriptor_t *current = current_process->file_descriptors;
    while (current != NULL) {
        if (current->descriptor_id == fd) {
            if (current->device->ioctl == NULL) {
                return -EOPNOTSUPP;
            }
            return current->device->ioctl(current->data, request, arg, current->device);
        }
        current = current->next;
    }
    return -EBADF;
}

/**
 * Initialize the filesystem.
 * 
 * @param ramdisk_device The device to use as the ramdisk
*/
void filesystem_init(device_t *ramdisk_device) {
    if (ramdisk_device == NULL) {
        kpanic("Ramdisk device not found, boot can't continue\n");
    }

    strcpy((char *)current_process->pwd, "/");

    // Mount the ramdisk
    mount_at("/mnt/ramdisk", ramdisk_device, "xandisk", 0);

    root_filesystem = NULL;
    current_process->file_descriptors = NULL;
}