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
#include <unused.h>
#include <process.h>

syscall_t syscall_table[512];

int64_t syscall_open(regs_t *regs) {
    printf("SYSCALL_OPEN: %s\n", (char *)regs->rdi);
    return fopen((char *)regs->rdi, regs->rsi, regs->rdx);
}

int64_t syscall_exit(regs_t *regs) {

    process_exit(regs->rdi);
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

int64_t syscall_chdir(regs_t *regs) {
    return fsetpwd((char *)regs->rdi);
}

int64_t syscall_getcwd(regs_t *regs) {
    return fgetpwd((char *)regs->rdi, regs->rsi);
}

int64_t syscall_fork(regs_t *regs) {
    UNUSED(regs);

    return fork();
}

int64_t syscall_execv(regs_t *regs) {
    return execv(regs);
}

void syscall_init() {
    for (int i = 0; i < 512; i++) {
        syscall_table[i] = NULL;
    }

    syscall_table[0] = &syscall_read;
    syscall_table[1] = &syscall_write;
    syscall_table[2] = &syscall_open;
    syscall_table[3] = &syscall_close;
    syscall_table[57] = &syscall_fork;
    syscall_table[59] = &syscall_execv;
    syscall_table[60] = &syscall_exit;
    syscall_table[79] = &syscall_getcwd;
    syscall_table[80] = &syscall_chdir;
    syscall_table[217] = &getdents64;
}

void syscall_handler(regs_t *regs)
{
    if (syscall_table[regs->rax] != NULL) {
        regs->rax = syscall_table[regs->rax](regs);
    } else {
        serial_printf("Unknown syscall: %d\n", regs->rax);
        regs->rax = -ENOSYS;
    }
}