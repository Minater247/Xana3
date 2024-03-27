#ifndef _ELF_LOADER_H
#define _ELF_LOADER_H

uint32_t load_elf64(char *elf_file);

extern uint32_t symbols_location;
extern uint32_t strings_location;
extern uint32_t symbols_count;

#endif