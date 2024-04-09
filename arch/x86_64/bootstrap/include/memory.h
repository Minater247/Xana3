#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdint.h>

#define KERNEL_LOAD_ADDR 0x800000

typedef struct {
    uint64_t entries[512];
    uint64_t virt[512]; //virtual addresses of the page tables
    bool is_full[512];
    uint64_t phys_addr;
} __attribute__((packed)) page_directory_t;

typedef struct {
    uint64_t pt_entry[512];
} __attribute__((packed)) page_table_t;

void memory_init();

extern uint32_t kheap_loc;

#endif