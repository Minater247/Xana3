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

typedef pointer_int_t (*open_func_t)(const char *, uint64_t, void *);
typedef size_t (*read_func_t)(void *, size_t, size_t, void *, void *, uint64_t);
typedef int (*close_func_t)(void *, void *);
typedef int (*fcntl_func_t)(int, long, void *, void *);
typedef size_t (*file_size_func_t)(const char *, void *);
typedef size_t (*write_func_t)(void *, size_t, size_t, void *, void *, uint64_t);
typedef size_t (*getdents64_func_t)(void *, size_t, void *, void *);
typedef off_t (*lseek_func_t)(void *, off_t, int, void *);
typedef int (*ioctl_func_t)(void *, unsigned long, void *, void *);

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

	file_size_func_t file_size;

	struct device *next;
} device_t;

device_t *register_device();
void init_device_device();

#endif