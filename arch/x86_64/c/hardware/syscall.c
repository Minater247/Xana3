#include <stdint.h>
#include <stdbool.h>

#include <system.h>
#include <display.h>
#include <errors.h>
#include <serial.h>
#include <filesystem.h>
#include <syscall.h>
#include <sys/errno.h>
#include <trace.h>

syscall_t syscall_table[512];

int64_t syscall_open(regs_t *regs) {
    return fopen((char *)regs->rdi, regs->rsi, regs->rdx);
    
}

int64_t syscall_exit(regs_t *regs) {
    // to be dealt with when multitasking is implemented

    printf("Process exited with code %d\n", regs->rdi);

    traceback(10);

    asm volatile ("sti");

    while (true);
}

int64_t syscall_write(regs_t *regs) {
    return fwrite((void *)regs->rsi, regs->rdx, 1, regs->rdi);
}

int64_t syscall_close(regs_t *regs) {
    return fclose(regs->rdi);
}

int64_t syscall_read(regs_t *regs) {
    return fread((void *)regs->rsi, regs->rdx, 1, regs->rdi);
}

int64_t getdents64(regs_t *regs) {
    return fgetdents64(regs->rdi, (void *)regs->rsi, regs->rdx);
}

void syscall_init() {
    for (int i = 0; i < 512; i++) {
        syscall_table[i] = NULL;
    }

    syscall_table[0] = &syscall_read;
    syscall_table[1] = &syscall_write;
    syscall_table[2] = &syscall_open;
    syscall_table[3] = &syscall_close;
    syscall_table[60] = &syscall_exit;
    syscall_table[217] = &getdents64;
}

int64_t syscall_handler(regs_t *regs)
{
    if (syscall_table[regs->rax] != NULL) {
        return syscall_table[regs->rax](regs);
    } else {
        printf("Unknown syscall: %d\n", regs->rax);
        while (true);
    }

    return -ENOSYS;
}