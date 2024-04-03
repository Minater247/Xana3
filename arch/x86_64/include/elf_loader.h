#ifndef _ELF_LOADER_H
#define _ELF_LOADER_H

#include <stdint.h>

uint32_t load_elf64(char *elf_file, page_directory_t *elf_pml4);

#endif