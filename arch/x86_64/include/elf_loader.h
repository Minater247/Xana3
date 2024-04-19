#ifndef _ELF_LOADER_H
#define _ELF_LOADER_H

#include <stdint.h>
#include <process.h>

typedef struct {
    uint64_t entry;
    uint64_t max_addr;
    int status;
    memregion_t *regions;
} elf_info_t;

elf_info_t load_elf64(char *elf_file, page_directory_t *elf_pml4);

#endif