#ifndef _MEMORY_H
#define _MEMORY_H

#include <stdint.h>
#include <stdbool.h>

#define INIT_HEAP_PAGES 512

typedef struct heap_header
{
    uint32_t magic;
    uint64_t length;
    bool free;
    struct heap_header *next;
    struct heap_header *prev;
} __attribute__((packed)) heap_header_t;

typedef struct page_table_entry {
    uint16_t pml4;
    uint16_t pdpt;
    uint16_t pd;
    uint16_t pt;
} __attribute__((packed)) page_table_entry_t;

typedef struct memory_region
{
    uint64_t start;
    uint64_t end;
    struct memory_region *next;
} __attribute__((packed)) memory_region_t;

typedef struct {
    uint64_t pt_entry[512];
} __attribute__((packed)) page_table_t;

typedef struct bitmap_pagetable
{
    uint8_t bitmap[512 / 8];
    uint64_t start_phys;
    struct bitmap_pagetable *next;
} __attribute__((packed)) bitmap_pagetable_t;

typedef struct {
    uint64_t entries[512];
    uint64_t virt[512]; //virtual addresses of the page tables
    bool is_full[512];
    uint64_t phys_addr;
} __attribute__((packed)) page_directory_t;

void memory_init(uint64_t old_kheap_end, uint64_t mmap_tag_addr, uint64_t framebuffer_tag_addr);
void *kmalloc(uint64_t size);
void *kmalloc_a(uint64_t size);
void *kmalloc_p(uint64_t size, uint64_t *phys);
void *kmalloc_ap(uint64_t size, uint64_t *phys);
void kfree(void *ptr);
void kfree_a(void *ptr);
void heap_dump();
page_table_entry_t first_free_page();
uint64_t first_free_page_addr();
bool map_page_kmalloc(uint64_t virt, uint64_t phys, bool is_kernel, bool is_writeable, page_directory_t *pml4_root);
page_directory_t *clone_page_directory(page_directory_t *directory);
void switch_page_directory(page_directory_t *directory);
void free_page_directory(page_directory_t *directory);
uint64_t virt_to_phys(uint64_t virt, page_directory_t *pd);
void free_page(uint64_t virt, page_directory_t *pd);
int memory_set_protection(void *addr, uint64_t length, uint64_t prot);
bool is_page_free(uint64_t virt);
void *krealloc(void *ptr, uint64_t size);

extern page_directory_t *current_pml4;

#endif