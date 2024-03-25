#ifndef _INFO_H
#define _INFO_H

#define VERSION_STRING "0.0.1"

#include <stdint.h>

typedef struct {
    uint32_t framebuffer_tag;
    uint32_t kheap_end;
    uint32_t ramdisk_addr;
    uint32_t mmap_tag_addr;
} kernel_info_t;

#endif