#include <stdint.h>
#include <stddef.h>

#include <display.h>
#include <errors.h>
#include <multiboot.h>
#include <info.h>
#include <errors.h>
#include <serial.h>
#include <tables.h>
#include <drivers/PIT.h>
#include <memory.h>
#include <cpuid_funcs.h>
#include <ramdisk.h>
#include <elf_loader.h>

void preboot_load(uint32_t magic, void *mbd) {
    serial_init(SERIAL_COM1, 38400);
    serial_printf("XanaduOS preboot loader %s\n", VERSION_STRING);

    kassert(load_multiboot(magic, mbd));

    printf("XanaduOS preboot loader %s\n", VERSION_STRING);

    enableBackground(true);

    // ANSI color testing
    printf("\033[0;30ma\033[0;31mb\033[0;32mc\033[0;33md\033[0;34me\033[0;35mf\033[0;36mg\033[0;37mh\033[0m\n");
    printf("\033[0;90ma\033[0;91mb\033[0;92mc\033[0;93md\033[0;94me\033[0;95mf\033[0;96mg\033[0;97mh\033[0m\n");
    printf("\033[30;40ma\033[30;41mb\033[30;42mc\033[30;43md\033[30;44me\033[30;45mf\033[30;46mg\033[30;47mh\033[0m\n");
    printf("\033[30;100ma\033[30;101mb\033[30;102mc\033[30;103md\033[30;104me\033[30;105mf\033[30;106mg\033[30;107mh\033[0m\n");

    tables_initialize();

    memory_init();

    kassert_msg(has_cpuid(), "CPU does not support CPUID\n");
    kassert_msg(has_long_mode(), "CPU does not support long mode\n");

    // Alright, let's get the kernel loaded from the ramdisk
    char *elf_file = ramdisk_get_path_data("/bin/kernel.bin", &boot_ramdisk);
    if (elf_file == NULL) {
        kpanic("Failed to load kernel from ramdisk\n");
    }

    uint32_t kernel_entry = load_elf64(elf_file);
    // if the highest bit is set, we have an error
    if (kernel_entry & 0x80000000) {
        kpanic("Failed to load kernel from ramdisk - error %d\n", kernel_entry);
    } else {
        printf("Kernel loaded at 0x%x\n", kernel_entry);
    }

    // Set long mode bit in MSR
    uint32_t msr;
    asm volatile("rdmsr" : "=a"(msr) : "c"(0xC0000080));
    msr |= 1 << 8;
    asm volatile("wrmsr" : : "a"(msr), "c"(0xC0000080));

    asm volatile("xchg %bx, %bx"); //bochs

    // Enable paging (cr3 is loaded)
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" : : "r"(cr0));

    while (1);

    // Jump to the kernel (addr in kernel_entry)
    asm volatile("jmp *%0" : : "r"(kernel_entry));
}