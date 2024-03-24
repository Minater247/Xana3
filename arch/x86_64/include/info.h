#ifndef _INFO_H
#define _INFO_H

#include <stdint.h>

typedef struct {
    uint32_t framebuffer_tag;
    uint32_t kheap_end;
    uint32_t ramdisk_addr;
} kernel_info_t;

#endif