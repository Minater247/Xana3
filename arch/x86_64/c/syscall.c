#include <stdint.h>
#include <stdbool.h>

#include <system.h>
#include <display.h>
#include <errors.h>
#include <serial.h>
#include <filesystem.h>
#include <syscall.h>

syscall_t syscall_table[512];

int64_t syscall_open(regs_t *regs) {
    printf("Path to open: %s\n", (char *)regs->rdi);
    return fopen((char *)regs->rdi, regs->rsi, regs->rdx);
}

int64_t syscall_exit(regs_t *regs) {
    // to be dealt with when multitasking is implemented

    while (1) {
        asm volatile ("hlt");
    }
}

void syscall_init() {
    for (int i = 0; i < 512; i++) {
        syscall_table[i] = NULL;
    }

    syscall_table[2] = &syscall_open;
    syscall_table[60] = &syscall_exit;
}

int64_t syscall_handler(regs_t *regs)
{

    printf("Syscall %d\n", regs->rax);

    if (syscall_table[regs->rax] != NULL) {
        return syscall_table[regs->rax](regs);
    }

    return 0;
}