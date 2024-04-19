#ifndef _DEVICE_H
#define _DEVICE_H

#include <stdint.h>
#include <sys/types.h>
#include <stddef.h>

#define NAME_MAX 255

typedef struct
{
	void *pointer;
	uint64_t value;
} pointer_int_t;

#define DEVICE_TYPE_DEVICES 0x0
#define DEVICE_TYPE_XANDISK 0x1
#define DEVICE_TYPE_SIMPLOU 0x2
#define DEVICE_TYPE_KYBOARD 0x3
#define DEVICE_TYPE_FRMEBUF 0x4

typedef pointer_int_t (*open_func_t)(const char *path, uint64_t flags, void *device_passed);
typedef size_t (*read_func_t)(void *ptr, size_t size, size_t nmemb, void *filedes_data, void *device_passed, uint64_t flags);
typedef int (*close_func_t)(void *filedes_data, void *device_passed);
typedef int (*fcntl_func_t)(int cmd, long arg, void *filedes_data, void *device);
typedef size_t (*file_size_func_t)(const char *path, void *device_passed);
typedef size_t (*write_func_t)(void *ptr, size_t size, size_t nmemb, void *filedes_data, void *device_passed, uint64_t flags);
typedef size_t (*getdents64_func_t)(void *ptr, size_t count, void *filedes_data, void *device_passed);
typedef off_t (*lseek_func_t)(void *filedes_data, off_t offset, int whence, void *device_passed);
typedef int (*ioctl_func_t)(void *filedes_data, unsigned long request, void *arg, void *device_passed);
typedef int (*stat_func_t)(void *filedes_data, void *buf, void *device_passed);
typedef void * (*dup_func_t)(void *filedes_data, void *device_passed);
typedef void * (*clone_func_t)(void *filedes_data, void *device_passed);

typedef struct device
{
	char name[NAME_MAX];
	dev_t id;
	uint32_t flags;
	void *data;
    uint32_t type;

	open_func_t open;
	read_func_t read;
	close_func_t close;
	fcntl_func_t fcntl;
    write_func_t write;
	getdents64_func_t getdents64;
	lseek_func_t lseek;
	ioctl_func_t ioctl;
	stat_func_t stat;
	dup_func_t dup;
	clone_func_t clone;

	file_size_func_t file_size;

	struct device *next;
} device_t;

device_t *register_device();
void init_device_device();

#endif