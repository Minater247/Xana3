#ifndef _MULTIBOOT_H
#define _MULTIBOOT_H

#include <stdint.h>
#include <stdbool.h>

bool load_multiboot(uint32_t magic, void *mbd);
uint32_t get_framebuffer_size_bytes(uint32_t tag_addr);
uint32_t get_framebuffer_addr(uint32_t tag_addr);

extern uint64_t multiboot_total_memory;
extern uint64_t multiboot_max_addr;
extern uint32_t framebuffer_tag;
extern uint32_t mmap_tag;
extern uint32_t elf_sections_tag;

#endif