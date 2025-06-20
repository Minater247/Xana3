#ifndef _INFO_H
#define _INFO_H

#define VERSION_STRING "0.0.1"

#include <stdint.h>

typedef struct {
    uint32_t kheap_end;
    uint32_t multiboot_addr;
    uint32_t elf_symbols_addr;
    uint32_t elf_strings_addr;
    uint32_t elf_symbol_count;
} kernel_info_t;

#endif