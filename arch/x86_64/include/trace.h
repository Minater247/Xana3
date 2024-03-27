#ifndef _TRACE_H
#define _TRACE_H

#include <stddef.h>

void traceback(size_t depth);
void traceback_init(uint32_t elf_symbols_addr, uint32_t elf_strings_addr, uint32_t elf_symbol_count);

#endif