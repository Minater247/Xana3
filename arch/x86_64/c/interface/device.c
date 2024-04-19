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
device_t *keyboard_devices = NULL;
device_t *framebuffer_devices = NULL;

device_t device_device;

void insert_device(device_t *list, device_t *device_to_insert)
{
    while (list->next != NULL)
    {
        list = list->next;
    }
    list->next = device_to_insert;
    device_to_insert->next = NULL;
    device_to_insert->id = list->id + 1;
}

device_t *register_device(device_t *device_to_register)
{
    if (device_to_register->type == DEVICE_TYPE_XANDISK)
    {
        device_t *current_device = xandisk_devices;
        if (current_device == NULL)
        {
            xandisk_devices = device_to_register;
            device_to_register->id = 0;
        }
        else
        {
            insert_device(current_device, device_to_register);
        }
    }
    else if (device_to_register->type == DEVICE_TYPE_SIMPLOU)
    {
        device_t *current_device = simpleo_devices;
        if (current_device == NULL)
        {
            simpleo_devices = device_to_register;
            device_to_register->id = 0;
        }
        else
        {
            insert_device(current_device, device_to_register);
        }
    }
    else if (device_to_register->type == DEVICE_TYPE_KYBOARD)
    {
        device_t *current_device = keyboard_devices;
        if (current_device == NULL)
        {
            keyboard_devices = device_to_register;
            device_to_register->id = 0;
        }
        else
        {
            insert_device(current_device, device_to_register);
        }
    }
    else if (device_to_register->type == DEVICE_TYPE_FRMEBUF)
    {
        device_t *current_device = framebuffer_devices;
        if (current_device == NULL)
        {
            framebuffer_devices = device_to_register;
            device_to_register->id = 0;
        }
        else
        {
            insert_device(current_device, device_to_register);
        }
    }

    return device_to_register;
}

typedef struct
{
    void *data;
    device_t *device;
    uint64_t dependents;
} device_open_data_t;

pointer_int_t device_open_helper(device_t *device_list, char *part, uint32_t first_number, char *path, uint64_t flags) {
    dev_t device_number = atoi(&part[first_number]);
    device_t *current_device = device_list;
    if (current_device == NULL) {
        return (pointer_int_t){NULL, -ENODEV};
    }
    while (current_device != NULL) {
        if (current_device->id == device_number) {
            device_open_data_t *data = (device_open_data_t *)kmalloc(sizeof(device_open_data_t));
            pointer_int_t open_data = current_device->open(path, flags, current_device);
            if (open_data.value != 0) {
                return open_data;
            }
            data->data = open_data.pointer;
            data->device = current_device;
            data->dependents++;
            return (pointer_int_t){data, 0};
        }
        current_device = current_device->next;
    }
    return (pointer_int_t){NULL, -ENODEV};
}

pointer_int_t device_open(char *path, uint64_t flags, void *device_passed)
{
    UNUSED(flags);
    UNUSED(device_passed);

    if (path[0] != '/')
    {
        return (pointer_int_t){NULL, -EINVAL};
    }

    uint32_t depth = get_path_depth(path);

    if (depth == 1)
    {
        char part[NAME_MAX];
        get_path_part(path, (char *)&part, 0);
        uint32_t first_number = 0;
        while ((part[first_number] < '0' || part[first_number] > '9') && !(part[first_number] == '\0'))
        {
            first_number++;
        }

        // if we didn't find a number, we can't open the device
        if (part[first_number] == '\0')
        {
            return (pointer_int_t){NULL, -ENODEV};
        }

        // now we can strncmp everything before the number to get the device name!
        // for now, only checking for "xd" (xandisk) and "so" (simple output)
        if (strncmp(part, "xd", 2) == 0)
        {
            return device_open_helper(xandisk_devices, part, first_number, path, flags);
        }
        else if (strncmp(part, "so", 2) == 0)
        {
            return device_open_helper(simpleo_devices, part, first_number, path, flags);
        }
        else if (strncmp(part, "kb", 2) == 0)
        {
            return device_open_helper(keyboard_devices, part, first_number, path, flags);
        }
        else if (strncmp(part, "fb", 2) == 0)
        {
            return device_open_helper(framebuffer_devices, part, first_number, path, flags);
        }
    }

    return (pointer_int_t){NULL, -ENODEV};
}

size_t device_write(void *data, size_t size, size_t count, device_open_data_t *device_to_write, device_t *this_device, uint64_t flags)
{
    UNUSED(this_device);

    return device_to_write->device->write(data, size, count, device_to_write->data, device_to_write->device, flags);
}

size_t device_read(void *data, size_t size, size_t count, device_open_data_t *device_to_read, device_t *this_device, uint64_t flags)
{
    UNUSED(this_device);

    return device_to_read->device->read(data, size, count, device_to_read->data, device_to_read->device, flags);
}

int device_close(device_open_data_t *data, device_t *this_device)
{
    UNUSED(this_device);

    if (data->device->close == NULL)
    {
        if (data->dependents > 1)
        {
            data->dependents--;
        } else {
            kfree(data);
        }
        return 0;
    }

    int ret = data->device->close(data->data, data->device);
    if (data->dependents > 1)
    {
        data->dependents--;
    } else {
        kfree(data);
    }
    return ret;
}

int device_ioctl(device_open_data_t *data, unsigned long request, void *arg, device_t *this_device)
{
    UNUSED(this_device);

    printf("Device IOCTL: 0x%lx\n", data->device->ioctl);

    if (data->device->ioctl == NULL)
    {
        return -ENOTTY;
    }

    return data->device->ioctl(data->data, request, arg, data->device);
}

off_t device_lseek(device_open_data_t *data, off_t offset, int whence, device_t *this_device)
{
    UNUSED(this_device);

    if (data->device->lseek == NULL)
    {
        return -ENOTSUP;
    }
    
    return data->device->lseek(data->data, offset, whence, data->device);
}

void *device_dup(device_open_data_t *data, device_t *this_device)
{
    UNUSED(this_device);

    data->dependents++;

    if (data->device->dup == NULL)
    {
        return NULL;
    }

    return data->device->dup(data->data, data->device);
}

void init_device_device()
{
    strcpy(device_device.name, "devices");
    device_device.flags = 0;
    device_device.data = NULL;
    device_device.next = NULL;

    // we need to cast the function pointer to return void *, like a function type taking path, flags, and device_t *, and returning a void *
    device_device.open = (open_func_t)device_open;
    device_device.read = (read_func_t)device_read;
    device_device.close = (close_func_t)device_close;
    device_device.fcntl = NULL;
    device_device.write = (write_func_t)device_write;
    device_device.file_size = NULL;
    device_device.lseek = (lseek_func_t)device_lseek;
    device_device.ioctl = (ioctl_func_t)device_ioctl;
    device_device.dup = (dup_func_t)device_dup;

    device_device.file_size = NULL;

    device_device.type = DEVICE_TYPE_DEVICES;

    mount_at("/dev", &device_device, "devices", 0);
}