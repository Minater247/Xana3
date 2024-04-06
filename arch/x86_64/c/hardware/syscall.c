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
#include <string.h>

syscall_t syscall_table[512];

int64_t syscall_open(regs_t *regs) {
    return fopen((char *)regs->rdi, regs->rsi, regs->rdx);
}

int64_t __attribute__((noreturn)) syscall_exit(regs_t *regs) {
    serial_printf("Process %d exited with status %d\n", current_process->pid, regs->rdi);

    process_exit(regs->rdi);

    kpanic("Process exited but process_exit() returned");
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

int64_t syscall_waitpid(regs_t *regs) {
    void *status = (void *)regs->rsi;
    int options = regs->rdx;
    pid_t pid = regs->rdi;

    return process_wait(WAIT_PID, pid, status, options);
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
    syscall_table[61] = &syscall_waitpid;
    syscall_table[79] = &syscall_getcwd;
    syscall_table[80] = &syscall_chdir;
    syscall_table[217] = &getdents64;
}


extern uint64_t syscall_old_rsp;
extern void syscall_stack_top;
extern void syscall_stack;
void syscall_handler(regs_t *regs)
{
    current_process->syscall_rsp = regs->rsp;

    // Copy current stack to process' syscall stack
    memcpy(current_process->syscall_stack, (void *)&syscall_stack, SYSCALL_STACK_SIZE);

    // Read and adjust rsp and rbp
    uint64_t rsp, rbp;
    ASM_READ_RSP(rsp);
    ASM_READ_RBP(rbp);
    uint64_t rsp_from_bottom = rsp - (uint64_t)&syscall_stack;
    uint64_t rbp_from_bottom = rbp - (uint64_t)&syscall_stack;

    // Move to the copied stack.
    // This is usually a *very bad* idea, but since the stack has been copied
    // verbatim, we can do this.
    ASM_WRITE_RSP((uint64_t)current_process->syscall_stack + rsp_from_bottom);
    ASM_WRITE_RBP((uint64_t)current_process->syscall_stack + rbp_from_bottom);

    // regs was based in the original stack, so we need to adjust it
    uint64_t offset = (uint64_t)current_process->syscall_stack - (uint64_t)&syscall_stack;
    regs = (regs_t *)((uint64_t)regs + offset);
    

    if (syscall_table[regs->rax] != NULL) {
        regs->rax = syscall_table[regs->rax](regs);
    } else {
        serial_printf("Unknown syscall: %d\n", regs->rax);
        regs->rax = -ENOSYS;
    }

    syscall_old_rsp = current_process->syscall_rsp;
}