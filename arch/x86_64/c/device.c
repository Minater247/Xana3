#include <stdint.h>
#include <stddef.h>

#include <device.h>
#include <filesystem.h>
#include <sys/errno.h>
#include <string.h>
#include <memory.h>
#include <serial.h>
#include <unused.h>
#include <display.h>

device_t *xandisk_devices = NULL;
device_t *simpleo_devices = NULL;

void insert_device(device_t *list, device_t *device_to_insert) {
    while (list->next != NULL) {
        list = list->next;
    }
    list->next = device_to_insert;
    device_to_insert->next = NULL;
    device_to_insert->id = list->id + 1;
}

device_t *register_device(device_t *device_to_register) {
    if (device_to_register->type == DEVICE_TYPE_XANDISK) {
        device_t *current_device = xandisk_devices;
        if (current_device == NULL) {
            xandisk_devices = device_to_register;
            device_to_register->id = 0;
        } else {
            insert_device(current_device, device_to_register);
        }
    } else if (device_to_register->type == DEVICE_TYPE_SIMPLOU) {
        device_t *current_device = simpleo_devices;
        if (current_device == NULL) {
            simpleo_devices = device_to_register;
            device_to_register->id = 0;
        } else {
            insert_device(current_device, device_to_register);
        }
    }

    return device_to_register;
}

pointer_int_t device_open(char *path, uint64_t flags, void *device_passed) {
    UNUSED(flags);
    UNUSED(device_passed);

    serial_printf("DEVICE_OPEN: %s\n", path);

    if (path[0] != '/') {
        return (pointer_int_t){NULL, -EINVAL};
    }

    uint32_t depth = get_path_depth(path);

    if (depth == 1) {
        char part[NAME_MAX];
        get_path_part(path, (char *)&part, 0);
        uint32_t first_number = 0;
        while ((part[first_number] < '0' || part[first_number] > '9') && !(part[first_number] == '\0')) {
            first_number++;
        }

        // if we didn't find a number, we can't open the device
        if (part[first_number] == '\0') {
            return (pointer_int_t){NULL, -ENODEV};
        }

        printf("First numeric char idx: %d\n", first_number);

        // now we can strncmp everything before the number to get the device name!
        // for now, only checking for "xd" (xandisk) and "so" (simple output)
        if (strncmp(part, "xd", 2) == 0) {
            // we have an xandisk device
            dev_t device_number = atoi(&part[first_number]);
            device_t *current_device = xandisk_devices;
            while (current_device != NULL) {
                if (current_device->id == device_number) {
                    device_open_data_t *data = (device_open_data_t *)kmalloc(sizeof(device_open_data_t));
                    data->data = current_device->open(path, flags, current_device);
                    data->device = current_device;
                    return (pointer_int_t){data, 0};
                }
                current_device = current_device->next;
            }
        } else if (strncmp(part, "so", 2) == 0) {
            // we have a simple output device
            dev_t device_number = atoi(&part[first_number]);
            device_t *current_device = simpleo_devices;
            while (current_device != NULL) {
                if (current_device->id == device_number) {
                    device_open_data_t *data = (device_open_data_t *)kmalloc(sizeof(device_open_data_t));
                    data->data = current_device->open(path, flags, current_device);
                    data->device = current_device;
                    return (pointer_int_t){data, 0};
                }
                current_device = current_device->next;
            }
        }
    }

    return (pointer_int_t){NULL, -1};
}

void init_device_device() {
    device_t *device_device = (device_t *)kmalloc(sizeof(device_t));
    strcpy(device_device->name, "devices");
    device_device->flags = 0;
    device_device->data = NULL;
    device_device->next = NULL;
    
    // we need to cast the function pointer to return void *, like a function type taking path, flags, and device_t *, and returning a void *
    device_device->open = (open_func_t)device_open;
    device_device->read = NULL;
    device_device->close = NULL;
    device_device->fcntl = NULL;

    device_device->file_size = NULL;

    device_device->type = DEVICE_TYPE_DEVICES;

    mount_at("/dev", device_device, "devices", 0);
}