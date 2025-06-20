#pragma once

#include <stdint.h>

#include <info.h>

void multiboot_init(kernel_info_t *info);
extern void *ramdisk_addr;