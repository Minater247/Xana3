#include <stdint.h>
#include <stddef.h>

#include <errors.h>
#include <multiboot.h>
#include <info.h>
#include <errors.h>
#include <serial.h>
#include <tables.h>
#include <drivers/PIT.h>
#include <memory.h>
#include <cpuid_funcs.h>
#include <elf_loader.h>
#include <display.h>

kernel_info_t passed_info;

extern void enable_sse();

void preboot_load(uint32_t magic, void *mbd) {
    serial_init(SERIAL_COM1, 38400);
    serial_printf("XanaduOS preboot loader %s\n", VERSION_STRING);

    kassert(load_multiboot(magic, mbd));

    tables_initialize();

    memory_init();

    kassert_msg(has_cpuid(), "CPU does not support CPUID\n");
    kassert_msg(has_long_mode(), "CPU does not support long mode\n");
    kassert_msg(has_sse(), "CPU does not support SSE\n");

    enable_sse();

    if (kernel_addr == NULL) {
        kpanic("Failed to load kernel from ramdisk\n");
    }

    uint32_t kernel_entry = load_elf64(kernel_addr);
    
    // if the highest bit is set, we have an error
    if (kernel_entry & 0x80000000) {
        kpanic("Failed to load kernel from ramdisk - error %d\n", kernel_entry);
    }

    // Set long mode bit in MSR
    uint32_t msr;
    asm volatile("rdmsr" : "=a"(msr) : "c"(0xC0000080));
    msr |= 1 << 8;
    asm volatile("wrmsr" : : "a"(msr), "c"(0xC0000080));

    // Enable paging (cr3 is loaded)
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" : : "r"(cr0));

    passed_info.kheap_end = kheap_loc;
    passed_info.multiboot_addr = (uint32_t)mbd;
    passed_info.elf_symbols_addr = symbols_location;
    passed_info.elf_strings_addr = strings_location;
    passed_info.elf_symbol_count = symbols_count;

    gdt_init64(kernel_entry);

    while (1);
}