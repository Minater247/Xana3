#include <stdint.h>

#include <multiboot.h>
#include <errors.h>
#include <memory.h>
#include <string.h>
#include <display.h>

uint32_t kheap_loc = 0;

page_directory_t *pml4_addr = NULL;

uint32_t kmalloc(uint32_t size) {
    // really basic bootstrap kmalloc
    kassert_msg(kheap_loc + size < 0x400000, "Out of memory");
    uint32_t ret = kheap_loc;
    kheap_loc += size;
    return ret;
}

uint32_t kmalloc_a(uint32_t size, uint32_t align) {
    // really basic bootstrap kmalloc (but now aligned)
    uint32_t aligned = (kheap_loc + align - 1) & ~(align - 1);
    kassert_msg((aligned + size) < 0x400000, "Out of memory");
    kheap_loc = aligned + size;
    return aligned;
}

bool map_page(uint64_t virt, uint64_t phys, bool is_kernel, bool is_writeable, page_directory_t *pml4_root)
{
    uint64_t pml4_index = (virt >> 39) & 0x1FF;
    uint64_t pdpt_index = (virt >> 30) & 0x1FF;
    uint64_t pd_index = (virt >> 21) & 0x1FF;
    uint64_t pt_index = (virt >> 12) & 0x1FF;

    if (pml4_root->virt[pml4_index] == 0)
    {
        uint32_t alloc_addr = kmalloc_a(sizeof(page_directory_t), 0x1000);
        pml4_root->virt[pml4_index] = (uint64_t)alloc_addr + 0xffffff8000000000;
        memset((void *)alloc_addr, 0, sizeof(page_directory_t));
        pml4_root->is_full[pml4_index] = false;
        pml4_root->entries[pml4_index] = alloc_addr | (is_writeable ? 1<<1 : 0) | (is_kernel ? 0 : 1<<2) | 1;
    }

    page_directory_t *pdpt = (page_directory_t *)(uint32_t)(pml4_root->virt[pml4_index]);
    if (pdpt->virt[pdpt_index] == 0)
    {
        uint32_t alloc_addr = kmalloc_a(sizeof(page_directory_t), 0x1000);
        pdpt->virt[pdpt_index] = (uint64_t)alloc_addr + 0xffffff8000000000;
        memset((void *)alloc_addr, 0, sizeof(page_directory_t));
        pdpt->is_full[pdpt_index] = false;
        pdpt->entries[pdpt_index] = alloc_addr | (is_writeable ? 1<<1 : 0) | (is_kernel ? 0 : 1<<2) | 1;
    }

    page_directory_t *pd = (page_directory_t *)(uint32_t)(pdpt->virt[pdpt_index]);
    if (pd->virt[pd_index] == 0)
    {
        uint32_t alloc_addr = kmalloc_a(sizeof(page_table_t), 0x1000);
        pd->virt[pd_index] = (uint64_t)alloc_addr + 0xffffff8000000000;
        memset((void *)alloc_addr, 0, sizeof(page_table_t));
        pd->is_full[pd_index] = false;
        pd->entries[pd_index] = alloc_addr | (is_writeable ? 1<<1 : 0) | (is_kernel ? 0 : 1<<2) | 1;
    }

    page_table_t *pt = (page_table_t *)(uint32_t)(pd->virt[pd_index]);
    if (pt->pt_entry[pt_index] != 0)
    {
        return false;
    }

    pt->pt_entry[pt_index] = (phys & 0xFFFFFFFFFFFFF000) | (is_writeable ? 1<<1 : 0) | (is_kernel ? 0 : 1<<2) | 1;

    return true;
}

void memory_init() {
    // Initialize the heap
    kheap_loc = (uint32_t)multiboot_max_addr;

    // Set up paging to multiboot_total_memory with offset of 0xffffff8000000000
    pml4_addr = (page_directory_t *)kmalloc_a(sizeof(page_directory_t), 0x1000);
    
    for (uint64_t addr = 0; addr < multiboot_total_memory; addr += 0x1000)
    {
        map_page(addr + 0xffffff8000000000, addr, true, true, pml4_addr);
    }

    // and now identity map from 0 to 0x800000
    for (uint64_t addr = 0; addr < 0x800000; addr += 0x1000)
    {
        map_page(addr, addr, true, true, pml4_addr);
    }

    // set the cr3 register
    asm volatile("mov %0, %%cr3" :: "r"((uint32_t)pml4_addr));

    // enable PAE
    uint32_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= 1<<5;
    asm volatile("mov %0, %%cr4" :: "r"(cr4));

    // Leave paging off for now
}