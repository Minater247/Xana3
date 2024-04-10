#ifndef _ELF_LOADER_H
#define _ELF_LOADER_H

#include <stdint.h>

typedef struct {
    uint64_t entry;
    uint64_t max_addr;
    int status;
} elf_info_t;

elf_info_t load_elf64(char *elf_file, page_directory_t *elf_pml4);

#endif