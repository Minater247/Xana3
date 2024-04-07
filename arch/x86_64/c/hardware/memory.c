#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <errors.h>
#include <memory.h>
#include <multiboot2.h>
#include <unused.h>
#include <tables.h>
#include <memory.h>
#include <system.h>
#include <string.h>
#include <serial.h>
#include <sys/mman.h>
#include <sys/errno.h>

heap_header_t *kheap = NULL;
uint64_t kheap_end = 0;

page_directory_t *kernel_pml4 __attribute__((aligned(4096)));
page_directory_t *current_pml4;

// preallocated space for things
page_directory_t *prealloc_pdpt;
page_directory_t *prealloc_pd;
page_table_t *prealloc_pt;

uint8_t *phys_mem_bitmap;

uint64_t total_memory;
uint64_t total_pages;

extern struct multiboot_tag_mmap *mmap_tag;
extern uint64_t boot_max_addr;
usable_memory_region_t *usable_memory_regions = NULL;

char *format_memory(uint64_t addr, char *buf)
{
    // print in B, KB, MB, GB
    if (addr < 1024)
    {
        itoa(addr, buf, 10);
        strcat(buf, "B");
    }
    else if (addr < 1024 * 1024)
    {
        itoa(addr / 1024, buf, 10);
        strcat(buf, "KB");
    }
    else if (addr < 1024 * 1024 * 1024)
    {
        itoa(addr / (1024 * 1024), buf, 10);
        strcat(buf, "MB");
    }
    else
    {
        itoa(addr / (1024 * 1024 * 1024), buf, 10);
        strcat(buf, "GB");
    }
    return buf;
}

void heap_dump_serial() {
    serial_printf("\nHEAP DUMP:\n");
    heap_header_t *header = kheap;
    while (header != NULL) {
        serial_printf("Header: 0x%lx\n", header);
        serial_printf("\tLength: 0x%lx\n", header->length);
        serial_printf("\tFree: %d\n", header->free);
        header = header->next;
    }
}

// typedef struct bitmap_1024
// {
//     uint64_t bitmap[16];
// } __attribute__((packed)) bitmap_1024_t;

// typedef struct {
//     uint64_t pt_entry[512];
// } __attribute__((packed)) page_table_t;

// typedef struct {
//     uint64_t entries[512];
//     uint64_t virt[512]; //virtual addresses of the page tables
//     bool is_full[512];
//     uint64_t phys_addr;
// } __attribute__((packed)) page_directory_t;

bool map_page_kmalloc(uint64_t virt, uint64_t phys, bool is_kernel, bool is_writeable, page_directory_t *pml4_root)
{
    uint64_t pml4_index = (virt >> 39) & 0x1FF;
    uint64_t pdpt_index = (virt >> 30) & 0x1FF;
    uint64_t pd_index = (virt >> 21) & 0x1FF;
    uint64_t pt_index = (virt >> 12) & 0x1FF;

    if (phys_mem_bitmap != NULL) {
        kassert_msg(!(phys_mem_bitmap[phys / 0x1000 / 8] & (1 << (phys / 0x1000 % 8))), "Attempted to map already used page!");
    }

    uint64_t phys_mapped;

    if (pml4_root->virt[pml4_index] == 0)
    {
        pml4_root->virt[pml4_index] = (uint64_t)kmalloc_ap(sizeof(page_directory_t), &phys_mapped);
        memset((void *)(pml4_root->virt[pml4_index]), 0, sizeof(page_directory_t));
        pml4_root->is_full[pml4_index] = false;
        pml4_root->entries[pml4_index] = phys_mapped | (is_writeable ? 1<<1 : 0) | (is_kernel ? 0 : 1<<2) | 1;
    }

    page_directory_t *pdpt = (page_directory_t *)(pml4_root->virt[pml4_index]);
    if (pdpt->virt[pdpt_index] == 0)
    {
        pdpt->virt[pdpt_index] = (uint64_t)kmalloc_ap(sizeof(page_directory_t), &phys_mapped);
        memset((void *)(pdpt->virt[pdpt_index]), 0, sizeof(page_directory_t));
        pdpt->is_full[pdpt_index] = false;
        pdpt->entries[pdpt_index] = phys_mapped | (is_writeable ? 1<<1 : 0) | (is_kernel ? 0 : 1<<2) | 1;
    }

    page_directory_t *pd = (page_directory_t *)(pdpt->virt[pdpt_index]);
    if (pd->virt[pd_index] == 0)
    {
        pd->virt[pd_index] = (uint64_t)kmalloc_ap(sizeof(page_table_t), &phys_mapped);
        memset((void *)(pd->virt[pd_index]), 0, sizeof(page_table_t));
        pd->is_full[pd_index] = false;
        pd->entries[pd_index] = phys_mapped | (is_writeable ? 1<<1 : 0) | (is_kernel ? 0 : 1<<2) | 1;
    }

    page_table_t *pt = (page_table_t *)(pd->virt[pd_index]);
    if (pt->pt_entry[pt_index] != 0)
    {
        return false;
    }

    pt->pt_entry[pt_index] = (phys & 0xFFFFFFFFFFFFF000) | (is_writeable ? 1<<1 : 0) | (is_kernel ? 0 : 1<<2) | 1;

    // Mark the physical page as used
    if (phys_mem_bitmap != NULL) {
        uint64_t page = phys / 0x1000;
        phys_mem_bitmap[page / 8] |= 1 << (page % 8);
    }

    return true;
}

bool map_page_prealloc(uint64_t virt, uint64_t phys, bool is_kernel, bool is_writeable, page_directory_t *pml4_root)
{
    uint64_t pml4_index = (virt >> 39) & 0x1FF;
    uint64_t pdpt_index = (virt >> 30) & 0x1FF;
    uint64_t pd_index = (virt >> 21) & 0x1FF;
    uint64_t pt_index = (virt >> 12) & 0x1FF;
    

    if (phys_mem_bitmap != NULL) {
        kassert_msg(!(phys_mem_bitmap[phys / 0x1000 / 8] & (1 << (phys / 0x1000 % 8))), "Attempted to map already used page; 0x%lx", phys);
    }

    if (pml4_root->virt[pml4_index] == 0)
    {
        kassert(prealloc_pdpt != NULL);
        pml4_root->virt[pml4_index] = (uint64_t)prealloc_pdpt;
        memset(prealloc_pdpt, 0, sizeof(page_directory_t));
        pml4_root->is_full[pml4_index] = false;
        pml4_root->entries[pml4_index] = virt_to_phys((uint64_t)prealloc_pdpt, pml4_root) | 0x3;
        prealloc_pdpt = NULL;
    }

    page_directory_t *pdpt = (page_directory_t *)(pml4_root->virt[pml4_index]);
    if (pdpt->virt[pdpt_index] == 0)
    {
        kassert(prealloc_pd != NULL);
        pdpt->virt[pdpt_index] = (uint64_t)prealloc_pd;
        memset(prealloc_pd, 0, sizeof(page_directory_t));
        pdpt->is_full[pdpt_index] = false;
        pdpt->entries[pdpt_index] = virt_to_phys((uint64_t)prealloc_pd, pml4_root) | 0x3;
        prealloc_pd = NULL;
    }

    page_directory_t *pd = (page_directory_t *)(pdpt->virt[pdpt_index]);
    if (pd->virt[pd_index] == 0)
    {
        kassert(prealloc_pt != NULL);
        pd->virt[pd_index] = (uint64_t)prealloc_pt;
        memset(prealloc_pt, 0, sizeof(page_table_t));
        pd->is_full[pd_index] = false;
        pd->entries[pd_index] = virt_to_phys((uint64_t)prealloc_pt, pml4_root) | 0x3;
        prealloc_pt = NULL;
    }

    page_table_t *pt = (page_table_t *)(pd->virt[pd_index]);
    if (pt->pt_entry[pt_index] != 0)
    {
        serial_printf("Failed to map page at 0x%lx\n", virt);
        return false;
    }

    pt->pt_entry[pt_index] = (phys & 0xFFFFFFFFFFFFF000) | (is_writeable ? 1<<1 : 0) | (is_kernel ? 0 : 1<<2) | 1;

    // Mark the physical page as used
    if (phys_mem_bitmap != NULL) {
        uint64_t page = phys / 0x1000;
        phys_mem_bitmap[page / 8] |= 1 << (page % 8);
    }

    return true;
}

bool is_page_free(uint64_t virt) {
    uint64_t pml4_index = (virt >> 39) & 0x1FF;
    uint64_t pdpt_index = (virt >> 30) & 0x1FF;
    uint64_t pd_index = (virt >> 21) & 0x1FF;
    uint64_t pt_index = (virt >> 12) & 0x1FF;

    if (current_pml4->virt[pml4_index] == 0)
    {
        return true;
    }

    page_directory_t *pdpt = (page_directory_t *)(current_pml4->virt[pml4_index]);
    if (pdpt->virt[pdpt_index] == 0)
    {
        return true;
    }

    page_directory_t *pd = (page_directory_t *)(pdpt->virt[pdpt_index]);
    if (pd->virt[pd_index] == 0)
    {
        return true;
    }

    page_table_t *pt = (page_table_t *)(pd->virt[pd_index]);
    if (pt->pt_entry[pt_index] == 0)
    {
        return true;
    }

    return false;

}

void memory_init(uint64_t old_kheap_end, uint64_t mmap_tag_addr, uint64_t framebuffer_tag_addr)
{
    kheap_end = old_kheap_end + VIRT_MEM_OFFSET;

    struct multiboot_tag_mmap *mmap_tag = (struct multiboot_tag_mmap *)mmap_tag_addr;

    total_memory = 0;
    multiboot_memory_map_t *mmap_entry;
    for (mmap_entry = mmap_tag->entries;
         (multiboot_uint8_t *)mmap_entry < (multiboot_uint8_t *)mmap_tag + mmap_tag->size;
         mmap_entry = (multiboot_memory_map_t *)((unsigned long)mmap_entry + mmap_tag->entry_size))
    {
        if (mmap_entry->type == MULTIBOOT_MEMORY_AVAILABLE)
        {
            usable_memory_region_t *new_region = kmalloc(sizeof(usable_memory_region_t));
            new_region->start = mmap_entry->addr;
            new_region->end = mmap_entry->addr + mmap_entry->len;
            new_region->next = usable_memory_regions;
            usable_memory_regions = new_region;
        }
        total_memory += mmap_entry->len;
    }

    char buf[15];
    printf("Booted with %s memory\n", format_memory(total_memory, buf));
    serial_printf("Booted with %s memory\n", format_memory(total_memory, buf));

    // We need to read cr3 to locate the current page table
    uint64_t cr3;
    ASM_GET_CR3(cr3);
    kernel_pml4 = (page_directory_t *)(cr3 + VIRT_MEM_OFFSET);
    current_pml4 = kernel_pml4;

    // Undo identity mapping
    kernel_pml4->virt[0] = 0;
    kernel_pml4->entries[0] = 0;

    // Set up preallocated space
    prealloc_pdpt = (page_directory_t *)kmalloc_a(sizeof(page_directory_t));
    prealloc_pd = (page_directory_t *)kmalloc_a(sizeof(page_directory_t));
    prealloc_pt = (page_table_t *)kmalloc_a(sizeof(page_table_t));
    memset(prealloc_pdpt, 0, sizeof(page_directory_t));
    memset(prealloc_pd, 0, sizeof(page_directory_t));
    memset(prealloc_pt, 0, sizeof(page_table_t));

    total_pages = total_memory / 0x1000;
    uint64_t bitmap_size = total_pages / 8;
    if (total_pages % 8 != 0)
    {
        bitmap_size++;
    }
    phys_mem_bitmap = (uint8_t *)kmalloc_a(bitmap_size);
    memset(phys_mem_bitmap, 0, bitmap_size);

    // Mark everything below kheap_end as used
    // 64-bit writes make this significantly faster
    uint64_t *bitmap_as_64 = (uint64_t *)phys_mem_bitmap;
    // align kheap_end to a page boundary
    kheap_end = (kheap_end & 0xFFF) ? (kheap_end & 0xFFFFFFFFFFFFF000) + 0x1000 : kheap_end;
    uint64_t pages_to_fill = (kheap_end - VIRT_MEM_OFFSET) / 0x1000 + 1;
    uint64_t pages_filled = 0;
    while (pages_filled + 64 < pages_to_fill) {
        bitmap_as_64[pages_filled / 64] = 0xFFFFFFFFFFFFFFFF;
        pages_filled += 64;
    }
    while (pages_filled < pages_to_fill) {
        phys_mem_bitmap[pages_filled / 8] |= 1 << (pages_filled % 8);
        pages_filled++;
    }

    // Also mark the framebuffer as used
    struct multiboot_tag_framebuffer *framebuffer_tag = (struct multiboot_tag_framebuffer *)framebuffer_tag_addr;
    uint64_t fb_start = framebuffer_tag->common.framebuffer_addr;
    uint64_t fb_end = fb_start + framebuffer_tag->common.framebuffer_pitch * framebuffer_tag->common.framebuffer_height;
    uint64_t fb_pages = (fb_end - fb_start) / 0x1000;
    for (uint64_t i = 0; i < fb_pages; i++) {
        phys_mem_bitmap[(fb_start / 0x1000 + i) / 8] |= 1 << ((fb_start / 0x1000 + i) % 8);
    }

    uint64_t last_mapped_virtaddr = total_memory + VIRT_MEM_OFFSET;
    // align up to 2MB boundary
    last_mapped_virtaddr = (last_mapped_virtaddr & 0x1FFFFF) ? (last_mapped_virtaddr & 0xFFFFFFFFFFE00000) + 0x200000 : last_mapped_virtaddr;

    // we will place the new kheap in physical memory after kheap_end, but in virtual memory after the last mapped address
    kheap = (heap_header_t *)(last_mapped_virtaddr);
    kheap_end = (uint64_t)kheap + (INIT_HEAP_PAGES * 0x1000) - 1;
    uint64_t new_map;
    
    for (uint64_t i = (uint64_t)kheap; i < kheap_end; i += 0x1000) {
        new_map = first_free_page_addr();
        kassert_msg(map_page_prealloc(i, new_map, true, true, kernel_pml4), "Failed to map page at 0x%lx", i);
    }

    kheap->magic = HEAP_MAGIC;
    kheap->length = (kheap_end - (uint64_t)kheap) - sizeof(heap_header_t);
    kheap->free = true;
    kheap->next = NULL;
    kheap->prev = NULL;

    if (prealloc_pd == NULL) {
        prealloc_pd = (page_directory_t *)kmalloc_a(sizeof(page_directory_t));
        memset(prealloc_pd, 0, sizeof(page_directory_t));
    }
    if (prealloc_pdpt == NULL) {
        prealloc_pdpt = (page_directory_t *)kmalloc_a(sizeof(page_directory_t));
        memset(prealloc_pdpt, 0, sizeof(page_directory_t));
    }
    if (prealloc_pt == NULL) {
        prealloc_pt = (page_table_t *)kmalloc_a(sizeof(page_table_t));
        memset(prealloc_pt, 0, sizeof(page_table_t));
    }

    serial_printf("Final state: %d%% of memory used\n", (first_free_page_addr() * 100) / total_memory);
    serial_printf("First free page: 0x%lx\n", first_free_page_addr());
}

bool is_in_usable_memory(uint64_t phys_addr)
{
    usable_memory_region_t *region;
    for (region = usable_memory_regions; region != NULL; region = region->next)
    {
        if (phys_addr >= region->start && phys_addr < region->end)
        {
            return true;
        }
    }
    return false;
}

uint64_t first_free_page_addr() {
    uint64_t *bitmap = (uint64_t *)phys_mem_bitmap;
    uint64_t first_free_bit = 0;
    bool found = false;

    while (!found) {
        while (bitmap[first_free_bit / 64] == 0xFFFFFFFFFFFFFFFF)
        {
            first_free_bit += 64;
        }

        if (first_free_bit > total_pages) {
            kpanic("Out of memory");
        }

        // __builtin_ctzll gets the index of the first set bit
        // so by inverting the bitmap, we can get the index of the first unset bit!
        first_free_bit += __builtin_ctzll(~bitmap[first_free_bit / 64]);

        if (first_free_bit > total_pages) {
            kpanic("Out of memory");
        }

        if (is_in_usable_memory(first_free_bit * 0x1000)) {
            break;
        }
    }
    
    return first_free_bit * 0x1000;
}

/**
 * Find the first n consecutive free pages
 *
 * @param n The number of consecutive pages to find
 * 
 * @return The address of the first page in the consecutive block
*/
uint64_t first_free_consec_page_addr(uint32_t n) {
    uint64_t *bitmap = (uint64_t *)phys_mem_bitmap;
    uint64_t first_free_bit = 0;
    uint32_t consecutive_free = 0;
    bool found = false;

    while (!found) {
        while (bitmap[first_free_bit / 64] == 0xFFFFFFFFFFFFFFFF)
        {
            first_free_bit += 64;
            consecutive_free = 0;
        }

        if (first_free_bit > total_pages) {
            kpanic("Out of memory");
        }

        first_free_bit += __builtin_ctzll(~bitmap[first_free_bit / 64]);

        if (first_free_bit > total_pages) {
            kpanic("Out of memory");
        }

        while (consecutive_free < n && !(bitmap[first_free_bit / 64] & (1ULL << (first_free_bit % 64)))) {
            first_free_bit++;
            consecutive_free++;
        }

        if (consecutive_free == n) {
            found = true;
        } else {
            consecutive_free = 0;
        }
    }

    return (first_free_bit - n) * 0x1000;
}

page_table_entry_t first_free_page()
{
    uint64_t addr = first_free_page_addr();
    return (page_table_entry_t){.pml4 = (addr >> 39) & 0x1FF, .pdpt = (addr >> 30) & 0x1FF, .pd = (addr >> 21) & 0x1FF, .pt = (addr >> 12) & 0x1FF};
}

void serial_heap_verify() {
    // print out the heap headers' address and length
    // if there is overlap or leakage, add an entry stating as such
    heap_header_t *header = kheap;
    uint64_t last_end = (uint64_t)kheap;
    while (header != NULL) {
        serial_printf("--------------------------------------------\n");
        if ((uint64_t)header < (uint64_t)kheap || (uint64_t)header > kheap_end) {
            serial_printf("Heap header at 0x%lx is outside of heap bounds\n", header);
        }
        if ((uint64_t)header + sizeof(heap_header_t) + header->length > kheap_end) {
            serial_printf("Heap header at 0x%lx is outside of heap bounds\n", header);
        }
        if ((uint64_t)header < last_end) {
            uint64_t overlap = last_end - (uint64_t)header;
            serial_printf("Heap header at 0x%lx overlaps with previous header by 0x%lx bytes\n", header, overlap);
        }
        if ((uint64_t)header > last_end) {
            uint64_t leakage = (uint64_t)header - last_end;
            serial_printf("Heap header at 0x%lx has a gap of 0x%lx bytes from previous header\n", header, leakage);
        }
        serial_printf("Header: 0x%lx\n", header);
        serial_printf("\tLength: 0x%lx\n", header->length);
        serial_printf("\tFree: %d\n", header->free);
        serial_printf("\tEnd: 0x%lx\n", (uint64_t)header + sizeof(heap_header_t) + header->length);
        last_end = (uint64_t)header + sizeof(heap_header_t) + header->length;
        header = header->next;
    }
    serial_printf("--------------------------------------------\n");
}

page_directory_t *clone_page_directory(page_directory_t *directory)
{
    page_directory_t *new_directory = (page_directory_t *)kmalloc_a(sizeof(page_directory_t));
    memset(new_directory, 0, sizeof(page_directory_t));
    new_directory->phys_addr = virt_to_phys((uint64_t)new_directory, kernel_pml4);

    for (uint64_t i = 0; i < 511; i++) {
        // actually copy everything not in the last entry (the kernel space)
        if (directory->virt[i] != 0) {
            page_directory_t *pdpt = (page_directory_t *)(directory->virt[i]);
            page_directory_t *new_pdpt = (page_directory_t *)kmalloc_a(sizeof(page_directory_t));
            memset(new_pdpt, 0, sizeof(page_directory_t));
            new_directory->virt[i] = (uint64_t)new_pdpt;
            // should be fine to use old PML4 since allocations here are in kernel space
            new_directory->entries[i] = virt_to_phys((uint64_t)new_pdpt, kernel_pml4) | (directory->entries[i] & 0xFFF);
            new_directory->is_full[i] = directory->is_full[i];

            for (uint64_t j = 0; j < 512; j++) {
                if (pdpt->virt[j] != 0) {
                    page_directory_t *pd = (page_directory_t *)(pdpt->virt[j]);
                    page_directory_t *new_pd = (page_directory_t *)kmalloc_a(sizeof(page_directory_t));
                    memset(new_pd, 0, sizeof(page_directory_t));
                    new_pdpt->virt[j] = (uint64_t)new_pd;
                    new_pdpt->entries[j] = virt_to_phys((uint64_t)new_pd, kernel_pml4) | (pdpt->entries[j] & 0xFFF);
                    new_pdpt->is_full[j] = pdpt->is_full[j];

                    for (uint64_t k = 0; k < 512; k++) {
                        if (pd->virt[k] != 0) {
                            page_table_t *pt = (page_table_t *)(pd->virt[k]);
                            page_table_t *new_pt = (page_table_t *)kmalloc_a(sizeof(page_table_t));
                            memset(new_pt, 0, sizeof(page_table_t));
                            new_pd->virt[k] = (uint64_t)new_pt;
                            new_pd->entries[k] = virt_to_phys((uint64_t)new_pt, kernel_pml4) | (pd->entries[k] & 0xFFF);
                            new_pd->is_full[k] = pd->is_full[k];

                            for (uint64_t l = 0; l < 512; l++) {
                                if (pt->pt_entry[l] != 0) {
                                    uint64_t new_phys = first_free_page_addr();

                                    // map the new page
                                    new_pt->pt_entry[l] = new_phys | (pt->pt_entry[l] & 0xFFF);

                                    // copy the old page to the new page
                                    memcpy((void *)(new_phys + VIRT_MEM_OFFSET), (void *)((pt->pt_entry[l] & 0xFFFFFFFFFFFFF000) + VIRT_MEM_OFFSET), 0x1000);

                                    serial_printf("Cloned virt 0x%lx (0x%lx) to 0x%lx\n", (i << 39) | (j << 30) | (k << 21) | (l << 12), pt->pt_entry[l] & 0xFFFFFFFFFFFFF000, new_phys);

                                    // mark the new page as used
                                    uint64_t page = new_phys / 0x1000;
                                    phys_mem_bitmap[page / 8] |= 1 << (page % 8);
                                }
                            }
                        } else if (pd->entries[k] & 1) {
                            unimplemented("2MB page cloning");
                        }
                    }
                }
            }
        
        }
    }

    // last entry is the kernel space, just copy it
    new_directory->virt[511] = directory->virt[511];
    new_directory->entries[511] = directory->entries[511];
    new_directory->is_full[511] = directory->is_full[511];

    return new_directory;
}

void serial_dump_mappings(page_directory_t *pml4, bool include_kernel) {
    serial_printf("Mappings for PML4 0x%lx\n", pml4);
    for (uint64_t i = 0; i < (include_kernel ? 512 : 511); i++) {
        if (pml4->virt[i] != 0) {
            page_directory_t *pdpt = (page_directory_t *)(pml4->virt[i]);
            for (uint64_t j = 0; j < 512; j++) {
                if (pdpt->virt[j] != 0) {
                    page_directory_t *pd = (page_directory_t *)(pdpt->virt[j]);
                    for (uint64_t k = 0; k < 512; k++) {
                        if (pd->virt[k] != 0) {
                            page_table_t *pt = (page_table_t *)(pd->virt[k]);
                            for (uint64_t l = 0; l < 512; l++) {
                                if (pt->pt_entry[l] != 0) {
                                    serial_printf("0x%lx -> 0x%lx\n", (i << 39) | (j << 30) | (k << 21) | (l << 12), pt->pt_entry[l] & 0xFFFFFFFFFFFFF000);
                                }
                            }
                        } else if (pd->entries[k] & (1 << 7)) {
                            serial_printf("2MB: 0x%lx -> 0x%lx\n", (i << 39) | (j << 30) | (k << 21), pd->entries[k] & 0xFFFFFFFFFFE00000);
                        }
                    }
                }
            }
        }
    }    
}

void free_page_directory(page_directory_t *directory)
{
    for (uint32_t i = 0; i < 511; i++)
    {
        if (directory->virt[i] != 0)
        {
            page_directory_t *pdpt = (page_directory_t *)(directory->virt[i]);
            for (uint32_t j = 0; j < 512; j++)
            {
                if (pdpt->virt[j] != 0)
                {
                    page_directory_t *pd = (page_directory_t *)(pdpt->virt[j]);
                    for (uint32_t k = 0; k < 512; k++)
                    {
                        if (pd->virt[k] != 0)
                        {
                            page_table_t *pt = (page_table_t *)(pd->virt[k]);
                            for (uint32_t l = 0; l < 512; l++)
                            {
                                if (pt->pt_entry[l] != 0)
                                {
                                    // mark the page as free
                                    uint64_t phys = pt->pt_entry[l] & 0xFFFFFFFFFFFFF000;
                                    uint64_t page = phys / 0x1000;
                                    phys_mem_bitmap[page / 8] &= ~(1 << (page % 8));
                                }
                            }
                            kfree_a((void *)pt);
                        }
                    }
                    kfree_a((void *)pd);
                }
            }
            kfree_a((void *)pdpt);
        }
    }
    kfree_a((void *)directory);
}

void switch_page_directory(page_directory_t *directory)
{
    current_pml4 = directory;
    ASM_SET_CR3(directory->phys_addr);
}

void free_page_addr(uint64_t virt, page_directory_t *pd)
{
    uint64_t phys = virt_to_phys(virt, pd);
    if (phys == (uint64_t)-1)
    {
        return;
    }
    
    // clear bit in bitmap
    uint64_t page = phys / 0x1000;
    phys_mem_bitmap[page / 8] &= ~(1 << (page % 8));

    uint64_t pml4_index = (virt >> 39) & 0x1FF;
    uint64_t pdpt_index = (virt >> 30) & 0x1FF;
    uint64_t pd_index = (virt >> 21) & 0x1FF;
    uint64_t pt_index = (virt >> 12) & 0x1FF;

    // Clear the page table entry
    page_directory_t *pdpt = (page_directory_t *)(pd->virt[pml4_index]);
    if (pdpt->virt[pdpt_index] == 0)
    {
        return;
    }

    page_directory_t *pd_ = (page_directory_t *)(pdpt->virt[pdpt_index]);
    if (pd_->virt[pd_index] == 0)
    {
        return;
    }

    page_table_t *pt = (page_table_t *)(pd_->virt[pd_index]);
    if (pt->pt_entry[pt_index] == 0)
    {
        return;
    }

    pt->pt_entry[pt_index] = 0;
}

void heap_expand()
{

    uint64_t old_end = kheap_end;
    // Expand heap by 2MB (1 page table of pages)
    for (uint64_t i = 0; i < 512; i++)
    {
        kassert_msg(map_page_prealloc(kheap_end + 1, first_free_page_addr(), true, true, kernel_pml4), "Failed to map page at 0x%lx", kheap_end + 1);
        kheap_end += 0x1000;
    }

    // Get the last heap header. If it's free, we can expand it
    heap_header_t *header = kheap;
    while (header->next != NULL)
    {
        header = header->next;
    }
    if (header->free)
    {
        header->length += 0x200000;
    }
    else
    {
        // Create a new header
        heap_header_t *new_header = (heap_header_t *)old_end;
        new_header->magic = HEAP_MAGIC;
        new_header->length = 0x200000 - sizeof(heap_header_t);
        new_header->free = true;
        new_header->next = NULL;
        new_header->prev = header;
        header->next = new_header;
    }



    if (prealloc_pt == NULL) {
        prealloc_pt = (page_table_t *)kmalloc_a(sizeof(page_table_t));
        memset(prealloc_pt, 0, sizeof(page_table_t));
    }

    if (prealloc_pd == NULL) {
        prealloc_pd = (page_directory_t *)kmalloc_a(sizeof(page_directory_t));
        memset(prealloc_pd, 0, sizeof(page_directory_t));
    }

    if (prealloc_pdpt == NULL) {
        prealloc_pdpt = (page_directory_t *)kmalloc_a(sizeof(page_directory_t));
        memset(prealloc_pdpt, 0, sizeof(page_directory_t));
    }
}

uint64_t virt_to_phys(uint64_t virt, page_directory_t *pd)
{
    uint64_t pml4_index = (virt >> 39) & 0x1FF;
    uint64_t pdpt_index = (virt >> 30) & 0x1FF;
    uint64_t pd_index = (virt >> 21) & 0x1FF;
    uint64_t pt_index = (virt >> 12) & 0x1FF;

    if (pd->virt[pml4_index] == 0)
    {
        return (uint64_t)-1;
    }

    page_directory_t *pdpt = (page_directory_t *)(pd->virt[pml4_index]);
    if (pdpt->virt[pdpt_index] == 0)
    {
        return (uint64_t)-1;
    }

    page_directory_t *pd_ = (page_directory_t *)(pdpt->virt[pdpt_index]);
    if (pd_->virt[pd_index] == 0)
    {
        // It still might be a valid address, 2mb pages
        if (pd_->entries[pd_index] & 1)
        {
            return (pd_->entries[pd_index] & 0xFFFFFFFFFFE00000) + (virt & 0x1FFFFF);
        }

        return (uint64_t)-1;
    }

    page_table_t *pt = (page_table_t *)(pd_->virt[pd_index]);
    if (pt->pt_entry[pt_index] == 0)
    {
        return (uint64_t)-1;
    }

    return (pt->pt_entry[pt_index] & 0xFFFFFFFFFFFFF000) + (virt & 0xFFF);
}

void __attribute__((malloc)) *kmalloc_int(uint64_t size, bool align, uint64_t *phys)
{
    if (kheap == NULL)
    {
        // simple alloc
        if (align)
        {
            kheap_end = (kheap_end & 0xFFF) ? (kheap_end & 0xFFFFFFFFFFFFF000) + 0x1000 : kheap_end;
        }
        uint64_t old_end = kheap_end;
        kheap_end += size;
        if (phys != NULL)
        {
            *phys = old_end - VIRT_MEM_OFFSET;
        }
        return (void *)old_end;
    }
    if (align)
    {
        heap_header_t *header;
        for (header = kheap; header != NULL; header = header->next)
        {
            uint64_t aligned_addr = (((uint64_t)header + sizeof(heap_header_t)) + 0xFFF) & 0xFFFFFFFFFFFFF000;
            if (((aligned_addr + size) <= ((uint64_t)header + sizeof(heap_header_t) + header->length)) && header->free)
            {
                // The aligned address block is within the header, and the header is free

                // Check if there is space before the aligned address block for another header
                if (aligned_addr - (uint64_t)header > sizeof(heap_header_t) + sizeof(heap_header_t))
                {
                    uint64_t new_header_address = aligned_addr - sizeof(heap_header_t);

                    uint64_t header_size_old = aligned_addr - sizeof(heap_header_t) - ((uint64_t)header + sizeof(heap_header_t));
                    uint64_t header_size_new = (uint64_t)header + sizeof(heap_header_t) + header->length - aligned_addr;

                    // this allocation needs a header
                    heap_header_t *new_header = (heap_header_t *)(new_header_address);
                    new_header->magic = HEAP_MAGIC;
                    new_header->length = header_size_new;
                    kassert(new_header->length != 0);
                    new_header->next = header->next;
                    new_header->prev = header;
                    new_header->free = false;

                    if (header->next != NULL)
                    {
                        header->next->prev = new_header;
                    }

                    header->next = new_header;
                    header->length = header_size_old;
                    kassert(header->length != 0);
                    header = new_header;

                    kassert(header->length >= size);
                }

                // check if there is space after the aligned address block for another header
                if (((uint64_t)header + sizeof(heap_header_t) + header->length) - (aligned_addr + size) > sizeof(heap_header_t))
                {
                    // there needs to be a header after this allocation
                    uint64_t new_header_address = aligned_addr + size;

                    uint64_t header_size_new = ((uint64_t)header + sizeof(heap_header_t) + header->length) - (aligned_addr + size + sizeof(heap_header_t));
                    uint64_t header_size_old = (aligned_addr + size) - ((uint64_t)header + sizeof(heap_header_t));

                    heap_header_t *new_header = (heap_header_t *)(new_header_address);
                    new_header->magic = HEAP_MAGIC;
                    new_header->length = header_size_new;
                    new_header->next = header->next;
                    new_header->prev = header;
                    new_header->free = true;

                    if (header->next != NULL)
                    {
                        header->next->prev = new_header;
                    }

                    header->next = new_header;
                    header->length = header_size_old;
                }

                header->free = false;

                if (phys != NULL)
                {
                    *phys = virt_to_phys(aligned_addr, kernel_pml4);
                }
                return (void *)aligned_addr;
            }
        }
    }
    else
    {
        heap_header_t *header;
        for (header = kheap; header != NULL; header = header->next)
        {
            if (header->length >= size && header->free)
            {
                // we found a header that's big enough
                // now we need to split it if it's too big
                if (header->length > size + sizeof(heap_header_t))
                {
                    // split the header
                    heap_header_t *new_header = (heap_header_t *)((uint64_t)header + sizeof(heap_header_t) + size);
                    new_header->magic = HEAP_MAGIC;
                    new_header->length = header->length - size - sizeof(heap_header_t);
                    new_header->next = header->next;
                    new_header->prev = header;
                    new_header->free = true;

                    if (header->next != NULL)
                    {
                        header->next->prev = new_header;
                    }

                    header->next = new_header;
                    header->length = size;
                }

                header->free = false;

                if (phys != NULL)
                {
                    *phys = virt_to_phys((uint64_t)header + sizeof(heap_header_t), kernel_pml4);
                }

                // now we can return the header
                return (void *)((uint64_t)header + sizeof(heap_header_t));
            }
        }
    }

    // We couldn't find a header that was big enough, so we need to expand the heap
    heap_expand();

    return kmalloc_int(size, align, phys);
}

void kfree_int(void *ptr, bool unaligned)
{
    kassert_msg(kheap != NULL, "kfree called before kheap initialization!");
    heap_header_t *header = (heap_header_t *)((uint64_t)ptr - sizeof(heap_header_t));
    if (!unaligned)
    {
        kassert_msg(header->magic == HEAP_MAGIC, "Invalid heap header magic number.");
    }
    else
    {
        // the header may be up to 0xFFF bytes before the pointer
        while (header->magic != HEAP_MAGIC)
        {
            header = (heap_header_t *)((uint64_t)header - 1);
        }
        if ((uint64_t)ptr - (uint64_t)header > 0xFFF)
        {
            kpanic("Header too far away!");
        }
    }
    header->free = true;
    // now we need to merge this header with the next header if it's free
    if (header->next != NULL && header->next->free)
    {
        header->length += header->next->length + sizeof(heap_header_t);
        header->next = header->next->next;
        if (header->next != NULL)
        {
            header->next->prev = header;
        }
    }

    // now we need to merge this header with the previous header if it's free
    if (header->prev != NULL && header->prev->free)
    {
        header->prev->length += header->length + sizeof(heap_header_t);
        header->prev->next = header->next;
        if (header->next != NULL)
        {
            header->next->prev = header->prev;
        }
    }
}

void kfree(void *ptr)
{
    kfree_int(ptr, false);
}

void kfree_a(void *ptr)
{
    kfree_int(ptr, true);
}

void *kmalloc_a(uint64_t size)
{
    return kmalloc_int(size, true, NULL);
}

void *kmalloc_p(uint64_t size, uint64_t *phys)
{
    return kmalloc_int(size, false, phys);
}

void *kmalloc_ap(uint64_t size, uint64_t *phys)
{
    return kmalloc_int(size, true, phys);
}

void *kmalloc(uint64_t size)
{
    return kmalloc_int(size, false, NULL);
}

void *krealloc(void *ptr, uint64_t size)
{
    heap_header_t *header = (heap_header_t *)((uint64_t)ptr - sizeof(heap_header_t));
    void *new_ptr = kmalloc(size);
    memcpy(new_ptr, ptr, header->length);
    kfree(ptr);
    return new_ptr;
}


int memory_set_protection(void *addr, uint64_t length, uint64_t prot)
{
    bool is_read = prot & PROT_READ;
    bool is_write = prot & PROT_WRITE;
    bool is_exec = prot & PROT_EXEC;

    uint64_t start = (uint64_t)addr;
    uint64_t end = start + length;
    uint64_t pages = (end - start) / 0x1000;
    if ((end - start) % 0x1000 != 0)
    {
        pages++;
    }

    for (uint64_t i = 0; i < pages; i++)
    {
        uint64_t page = start + (i * 0x1000);
        uint64_t pml4_index = (page >> 39) & 0x1FF;
        uint64_t pdpt_index = (page >> 30) & 0x1FF;
        uint64_t pd_index = (page >> 21) & 0x1FF;
        uint64_t pt_index = (page >> 12) & 0x1FF;

        if (current_pml4->virt[pml4_index] == 0)
        {
            return -ENOMEM;
        }

        page_directory_t *pdpt = (page_directory_t *)(current_pml4->virt[pml4_index]);
        if (pdpt->virt[pdpt_index] == 0)
        {
            return -ENOMEM;
        }

        page_directory_t *pd = (page_directory_t *)(pdpt->virt[pdpt_index]);
        if (pd->virt[pd_index] == 0)
        {
            return -ENOMEM;
        }

        page_table_t *pt = (page_table_t *)(pd->virt[pd_index]);
        if (pt->pt_entry[pt_index] == 0)
        {
            return -ENOMEM;
        }

        uint64_t entry = pt->pt_entry[pt_index];
        // bit 63 is execute-disable, bit 1 is writeable, bit 0 is present
        entry = (entry & 0x7FFFFFFFFFFFFFFC) | (is_exec ? 0 : 1ULL << 63) | (is_write ? 1 << 1 : 0) | (is_read ? 1 : 0);

        pt->pt_entry[pt_index] = entry;
    }

    return 0;
}

int64_t heap_free_space()
{
    heap_header_t *header = kheap;
    int64_t free_space = 0;
    while (header != NULL)
    {
        if (header->free)
        {
            free_space += header->length;
        }
        header = header->next;
    }
    return free_space;
}