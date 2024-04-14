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

uint64_t syscall_open(regs_t *regs) {
    return (uint64_t)fopen((char *)regs->rdi, regs->rsi, regs->rdx);
}

uint64_t __attribute__((noreturn)) syscall_exit(regs_t *regs) {
    serial_printf("Process %d exited with status %d\n", current_process->pid, regs->rdi);

    process_exit(regs->rdi);

    kpanic("Process exited but process_exit() returned");
}

uint64_t syscall_write(regs_t *regs) {
    return (uint64_t)fwrite((void *)regs->rsi, regs->rdx, 1, regs->rdi);
}

uint64_t syscall_close(regs_t *regs) {
    return (uint64_t)fclose(regs->rdi);
}

uint64_t syscall_read(regs_t *regs) {
    return (uint64_t)fread((void *)regs->rsi, regs->rdx, 1, regs->rdi);
}

uint64_t getdents64(regs_t *regs) {
    return (uint64_t)fgetdents64(regs->rdi, (void *)regs->rsi, regs->rdx);
}

uint64_t syscall_chdir(regs_t *regs) {
    return (uint64_t)fsetpwd((char *)regs->rdi);
}

uint64_t syscall_getcwd(regs_t *regs) {
    return (uint64_t)fgetpwd((char *)regs->rdi, regs->rsi);
}

uint64_t syscall_fork(regs_t *regs) {
    UNUSED(regs);

    return (uint64_t)fork();
}

uint64_t syscall_execv(regs_t *regs) {
    return (uint64_t)execv(regs);
}

uint64_t syscall_wait4(regs_t *regs) {
    void *status = (void *)regs->rsi;
    int options = regs->rdx;
    pid_t pid = regs->rdi;
    void *rusage = (void *)regs->r10;

    return (uint64_t)process_wait(pid, status, options, rusage);
}

uint64_t syscall_lseek(regs_t *regs) {
    return (uint64_t)flseek(regs->rdi, regs->rsi, regs->rdx);
}

uint64_t syscall_ioctl(regs_t *regs) {
    return (uint64_t)ioctl(regs->rdi, regs->rsi, (void *)regs->rdx);
}

uint64_t syscall_brk(regs_t *regs) {
    return (uint64_t)brk(regs->rdi);
}

uint64_t syscall_fstat(regs_t *regs) {
    return (uint64_t)fstat(regs->rdi, (struct stat *)regs->rsi);
}

uint64_t syscall_rt_sigaction(regs_t *regs) {
    return (uint64_t)rt_sigaction(regs->rdi, (const struct sigaction *)regs->rsi, (struct sigaction *)regs->rdx);
}

uint64_t __attribute__((noreturn)) syscall_rt_sigret(regs_t *regs) {
    UNUSED(regs);
    rt_sigret();
    kpanic("rt_sigret() returned");
}

void syscall_init() {
    for (int i = 0; i < 512; i++) {
        syscall_table[i] = NULL;
    }

    syscall_table[0] = &syscall_read;
    syscall_table[1] = &syscall_write;
    syscall_table[2] = &syscall_open;
    syscall_table[3] = &syscall_close;
    syscall_table[5] = &syscall_fstat;
    syscall_table[8] = &syscall_lseek;
    syscall_table[12] = &syscall_brk;
    syscall_table[13] = &syscall_rt_sigaction;
    syscall_table[15] = &syscall_rt_sigret;
    syscall_table[16] = &syscall_ioctl;
    syscall_table[57] = &syscall_fork;
    syscall_table[59] = &syscall_execv;
    syscall_table[60] = &syscall_exit;
    syscall_table[61] = &syscall_wait4;
    syscall_table[79] = &syscall_getcwd;
    syscall_table[80] = &syscall_chdir;
    syscall_table[217] = &getdents64;
}


extern uint64_t syscall_old_rsp;
extern uint8_t syscall_stack_top;
extern uint8_t syscall_stack;
uint64_t syscall_handler(regs_t *regs)
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
    // verbatim, and we don't really have to care about previous base pointer
    // values here, this should be fine.
    ASM_WRITE_RSP((uint64_t)current_process->syscall_stack + rsp_from_bottom);
    ASM_WRITE_RBP((uint64_t)current_process->syscall_stack + rbp_from_bottom);

    current_process->syscall_registers = *regs;
    serial_printf("Syscall %d started with RSP %lx\n", regs->rax, current_process->syscall_rsp);

    if (syscall_table[regs->rax] != NULL) {
        current_process->in_syscall = true;
        uint64_t raxval = syscall_table[regs->rax]((regs_t *)&(current_process->syscall_registers));
        current_process->syscall_registers.rax = raxval;
        current_process->in_syscall = false;
    } else {
        serial_printf("Unknown syscall: %d\n", regs->rax);
        process_exit_abnormal((exit_status_bits_t){.exit_status = 0, .stop_signal = SIGSYS, .normal_exit = false});
    }

    syscall_old_rsp = current_process->syscall_rsp;
    serial_printf("...finished with RSP %lx\n", syscall_old_rsp);

    // move the address of the current process registers to rax
    asm volatile("mov %0, %%rax" ::"r"(&current_process->syscall_registers));
    // jump to after_syscall
    asm volatile("jmp after_syscall");

    while (1)
        ;
}