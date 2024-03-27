#include <stdint.h>

#include <elf64.h>
#include <string.h>
#include <serial.h>
#include <memory.h>

uint32_t load_elf64(char *elf_file) {
    Elf64_Ehdr *header = (Elf64_Ehdr *)elf_file;
    if (header->e_ident[0] != 0x7F || header->e_ident[1] != 'E' || header->e_ident[2] != 'L' || header->e_ident[3] != 'F') {
        return (uint32_t)-1;
    }

    if (header->e_ident[EI_CLASS] != ELFCLASS64) {
        return (uint32_t)-2;
    }

    if (header->e_ident[EI_DATA] != ELFDATA2LSB) {
        return (uint32_t)-3;
    }

    if (header->e_ident[EI_VERSION] != EV_CURRENT) {
        return (uint32_t)-4;
    }

    if (header->e_type != ET_EXEC) {
        return (uint32_t)-5;
    }

    if (header->e_machine != EM_X86_64) {
        return (uint32_t)-6;
    }

    if (header->e_version != EV_CURRENT) {
        return (uint32_t)-7;
    }

    // Load the program headers
    for (int i = 0; i < header->e_phnum; i++) {
        Elf64_Phdr *phdr = (Elf64_Phdr *)(elf_file + header->e_phoff + (i * header->e_phentsize));
        // serial_printf("Program header %d: type %d, offset 0x%lx, vaddr 0x%lx, paddr 0x%lx, filesz 0x%lx, memsz 0x%lx, flags 0x%x, align 0x%lx\n",
        //        i, phdr->p_type, phdr->p_offset, phdr->p_vaddr, phdr->p_paddr, phdr->p_filesz, phdr->p_memsz, phdr->p_flags, phdr->p_align);

        if (phdr->p_type == PT_LOAD) {
            map_page_kmalloc(phdr->p_vaddr, first_free_page_addr(), false, true, current_pml4);

            // Copy the segment to the physical memory address
            memcpy((void *)phdr->p_paddr, (void *)(elf_file + phdr->p_offset), phdr->p_filesz);

            // Zero out the remaining memory if the memory size is larger than the file size
            if (phdr->p_memsz > phdr->p_filesz) {
                memset((void *)(phdr->p_paddr + phdr->p_filesz), 0, phdr->p_memsz - phdr->p_filesz);
            }
        }
    }

    return header->e_entry;
}