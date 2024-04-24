#include <stdint.h>

#include <elf64.h>
#include <string.h>
#include <serial.h>
#include <memory.h>
#include <elf_loader.h>
#include <errors.h>
#include <process.h>

elf_info_t load_elf64(char *elf_file, page_directory_t *elf_pml4) {
    Elf64_Ehdr *header = (Elf64_Ehdr *)elf_file;
    uint64_t max_addr = 0;
    if (header->e_ident[0] != 0x7F || header->e_ident[1] != 'E' || header->e_ident[2] != 'L' || header->e_ident[3] != 'F') {
        serial_printf("Invalid ELF magic number!\n");
        return (elf_info_t){0, 0, -1, NULL};
    }

    if (header->e_ident[EI_CLASS] != ELFCLASS64) {
        serial_printf("Invalid ELF class!\n");
        return (elf_info_t){0, 0, -2, NULL};
    }

    if (header->e_ident[EI_DATA] != ELFDATA2LSB) {
        serial_printf("Invalid ELF data encoding!\n");
        return (elf_info_t){0, 0, -3, NULL};
    }

    if (header->e_ident[EI_VERSION] != EV_CURRENT) {
        serial_printf("Invalid ELF version!\n");
        return (elf_info_t){0, 0, -4, NULL};
    }

    if (header->e_type != ET_EXEC) {
        serial_printf("Invalid ELF type!\n");
        return (elf_info_t){0, 0, -5, NULL};
    }

    if (header->e_machine != EM_X86_64) {
        serial_printf("Invalid ELF machine!\n");
        return (elf_info_t){0, 0, -6, NULL};
    }

    if (header->e_version != EV_CURRENT) {
        serial_printf("Invalid ELF version!\n");
        return (elf_info_t){0, 0, -7, NULL};
    }

    page_directory_t *old_pml4 = current_pml4;
    switch_page_directory(elf_pml4);

    memregion_t *regions = NULL;

    // Load the program headers
    for (int i = 0; i < header->e_phnum; i++) {
        Elf64_Phdr *phdr = (Elf64_Phdr *)(elf_file + header->e_phoff + (i * header->e_phentsize));
        // serial_printf("Program header %d: type %d, offset 0x%lx, vaddr 0x%lx, paddr 0x%lx, filesz 0x%lx, memsz 0x%lx, flags 0x%x, align 0x%lx\n",
        //        i, phdr->p_type, phdr->p_offset, phdr->p_vaddr, phdr->p_paddr, phdr->p_filesz, phdr->p_memsz, phdr->p_flags, phdr->p_align);

        if (phdr->p_type == PT_LOAD) {
            // Calculate the number of pages
            int num_pages = phdr->p_memsz / 0x1000;
            if (phdr->p_memsz % 0x1000 != 0) {
                num_pages++;
            }

            // Map each page
            for (int j = 0; j < num_pages; j++) {
                map_page_kmalloc(phdr->p_vaddr + j * 0x1000, first_free_page_addr(), false, true, elf_pml4);
            }

            // Copy the segment to the physical memory address
            memcpy((void *)phdr->p_paddr, (void *)(elf_file + phdr->p_offset), phdr->p_filesz);

            // Zero out the remaining memory if the memory size is larger than the file size
            if (phdr->p_memsz > phdr->p_filesz) {
                memset((void *)(phdr->p_paddr + phdr->p_filesz), 0, phdr->p_memsz - phdr->p_filesz);
            }

            // Keep track of the highest address we've loaded
            if (phdr->p_vaddr + phdr->p_memsz > max_addr) {
                max_addr = phdr->p_vaddr + phdr->p_memsz;
            }

            // Add the region to the list
            memregion_t *region = kmalloc(sizeof(memregion_t));
            region->start = phdr->p_vaddr;
            region->end = phdr->p_vaddr + phdr->p_memsz;
            region->flags = phdr->p_flags & 0x7;
        }
    }

    switch_page_directory(old_pml4);

    elf_info_t info;
    info.entry = header->e_entry;
    info.max_addr = max_addr;
    info.status = 0;
    info.regions = regions;

    return info;
}