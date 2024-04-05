#include <stdint.h>
#include <stddef.h>

#include <display.h>
#include <multiboot2.h>
#include <elf64.h>
#include <errors.h>
#include <system.h>
#include <serial.h>

extern struct multiboot_tag_elf_sections *elf_sections;

// variable to store the string table
char *string_table;
Elf64_Sym *symbol_table;
size_t symbol_count;

void traceback_init(uint32_t elf_symbols_addr, uint32_t elf_strings_addr, uint32_t elf_symbol_count)
{
    string_table = (char *)((uint64_t)elf_strings_addr + VIRT_MEM_OFFSET);
    symbol_table = (Elf64_Sym *)((uint64_t)elf_symbols_addr + VIRT_MEM_OFFSET);
    symbol_count = elf_symbol_count;
}

// TODO: really broken for some reason, tends to page fault
//       fix this!
bool traceback_failed = false;

void serial_traceback(size_t depth) {
    if (traceback_failed) {
        serial_printf("Panic in traceback, aborting\n");
        printf("(panic in traceback? something went horribly wrong)\n");
        return;
    }

    traceback_failed = true;

    uint64_t *rbp;
    ASM_READ_RBP(rbp);
    while (rbp != NULL) {
        uint64_t rip = rbp[1];
        serial_printf("0x%lx", rip);
        rbp = (uint64_t *)rbp[0];
        if (depth-- == 0) {
            break;  
        }
        if (rip == 0 || rip < VIRT_MEM_OFFSET) {
            break;
        }
        
        if (string_table == NULL || symbol_table == NULL) {
            continue;
        }

        bool found = false;
        // find the symbol that contains the RIP
        for (size_t i = 0; i < symbol_count; i++) {
            if (rip >= symbol_table[i].st_value && rip < symbol_table[i].st_value + symbol_table[i].st_size) {
                printf("  %s+0x%lx\n", string_table + symbol_table[i].st_name, rip - symbol_table[i].st_value);
                found = true;
                break;
            }
        }
        if (!found) {
            // find the closest symbol before the RIP
            uint64_t closest_diff = UINT64_MAX;
            size_t closest_index = 0;
            for (size_t i = 0; i < symbol_count; i++) {
                if (rip > symbol_table[i].st_value && rip - symbol_table[i].st_value < closest_diff) {
                    closest_diff = rip - symbol_table[i].st_value;
                    closest_index = i;
                }
            }
            serial_printf("  %s+0x%lx (asm)\n", string_table + symbol_table[closest_index].st_name, rip - symbol_table[closest_index].st_value);
        }
    }

    if (depth == 0) {
        serial_printf("...\n");
    } else if (rbp == NULL) {
        serial_printf("End of stack\n");
    }

    traceback_failed = false;
}

void traceback(size_t depth) {
    if (traceback_failed) {
        serial_printf("Panic in traceback, aborting\n");
        printf("(panic in traceback? something went horribly wrong)\n");
        return;
    }

    traceback_failed = true;

    uint64_t *rbp;
    ASM_READ_RBP(rbp);
    while (rbp != NULL) {
        uint64_t rip = rbp[1];
        printf("\033[36;40m0x%lx", rip);
        rbp = (uint64_t *)rbp[0];
        if (depth-- == 0) {
            break;  
        }
        if (rip == 0 || rip < VIRT_MEM_OFFSET) {
            break;
        }
        
        if (string_table == NULL || symbol_table == NULL) {
            continue;
        }

        bool found = false;
        // find the symbol that contains the RIP
        for (size_t i = 0; i < symbol_count; i++) {
            if (rip >= symbol_table[i].st_value && rip < symbol_table[i].st_value + symbol_table[i].st_size) {
                printf("\033[32m  %s+0x%lx\n", string_table + symbol_table[i].st_name, rip - symbol_table[i].st_value);
                found = true;
                break;
            }
        }
        if (!found) {
            // find the closest symbol before the RIP
            uint64_t closest_diff = UINT64_MAX;
            size_t closest_index = 0;
            for (size_t i = 0; i < symbol_count; i++) {
                if (rip > symbol_table[i].st_value && rip - symbol_table[i].st_value < closest_diff) {
                    closest_diff = rip - symbol_table[i].st_value;
                    closest_index = i;
                }
            }
            printf("\033[33m  %s+0x%lx (asm)\n", string_table + symbol_table[closest_index].st_name, rip - symbol_table[closest_index].st_value);
        }
    }

    if (depth == 0) {
        printf("\033[34m...\n\033[0m");
    } else if (rbp == NULL) {
        printf("\033[36mEnd of stack\n\033[0m");
    }

    traceback_failed = false;
}