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
#include <tables.h>

syscall_t syscall_table[512];

uint64_t syscall_open(regs_t *regs) {
    return (uint64_t)kfopen((char *)regs->rdi, regs->rsi, regs->rdx);
}

uint64_t __attribute__((noreturn)) syscall_exit(regs_t *regs) {
    serial_printf("Process %d exited with status %d\n", current_process->pid, regs->rdi);

    process_exit(regs->rdi);

    kpanic("Process exited but process_exit() returned");
}

uint64_t syscall_write(regs_t *regs) {
    return (uint64_t)kfwrite((void *)regs->rsi, regs->rdx, 1, regs->rdi);
}

uint64_t syscall_close(regs_t *regs) {
    return (uint64_t)kfclose(regs->rdi);
}

uint64_t syscall_read(regs_t *regs) {
    return (uint64_t)kfread((void *)regs->rsi, regs->rdx, 1, regs->rdi);
}

uint64_t getdents64(regs_t *regs) {
    return (uint64_t)kfgetdents64(regs->rdi, (void *)regs->rsi, regs->rdx);
}

uint64_t syscall_chdir(regs_t *regs) {
    return (uint64_t)kfsetpwd((char *)regs->rdi);
}

uint64_t syscall_getcwd(regs_t *regs) {
    return (uint64_t)kfgetpwd((char *)regs->rdi, regs->rsi);
}

uint64_t syscall_fork(regs_t *regs) {
    UNUSED(regs);

    return (uint64_t)kfork();
}

uint64_t syscall_execv(regs_t *regs) {
    return (uint64_t)kexecv(regs);
}

uint64_t syscall_wait4(regs_t *regs) {
    void *status = (void *)regs->rsi;
    int options = regs->rdx;
    pid_t pid = regs->rdi;
    void *rusage = (void *)regs->r10;

    return (uint64_t)process_wait(pid, status, options, rusage);
}

uint64_t syscall_lseek(regs_t *regs) {
    return (uint64_t)kflseek(regs->rdi, regs->rsi, regs->rdx);
}

uint64_t syscall_ioctl(regs_t *regs) {
    serial_printf("IOCTL called from 0x%lx: ioctl(%d, 0x%lx, 0x%lx)\n", regs->rcx, regs->rdi, regs->rsi, regs->rdx);
    return (uint64_t)kioctl(regs->rdi, regs->rsi, (void *)regs->rdx);
}

uint64_t syscall_brk(regs_t *regs) {
    return (uint64_t)kbrk(regs->rdi);
}

uint64_t syscall_fstat(regs_t *regs) {
    return (uint64_t)kfstat(regs->rdi, (struct stat *)regs->rsi);
}

uint64_t syscall_rt_sigaction(regs_t *regs) {
    return (uint64_t)krt_sigaction(regs->rdi, (const struct sigaction *)regs->rsi, (struct sigaction *)regs->rdx);
}

uint64_t __attribute__((noreturn)) syscall_rt_sigret(regs_t *regs) {
    UNUSED(regs);
    krt_sigret(regs->rdi); // Custom: rdi contains 0 if this was a regular signal, or 1 if this was an after-syscall signal
    kpanic("rt_sigret() returned");
}

uint64_t syscall_kill(regs_t *regs) {
    return (uint64_t)process_kill(regs->rdi, regs->rsi);
}

uint64_t syscall_getpid(regs_t *regs) {
    UNUSED(regs);

    return (uint64_t)current_process->pid;
}

uint64_t syscall_getppid(regs_t *regs) {
    UNUSED(regs);

    return (uint64_t)current_process->ppid;
}

uint64_t syscall_getpgrp(regs_t *regs) {
    UNUSED(regs);

    return (uint64_t)current_process->pgid;
}

uint64_t syscall_stat(regs_t *regs) {
    return (uint64_t)kstat((char *)regs->rdi, (struct stat *)regs->rsi);
}

uint64_t syscall_timeofday(regs_t *regs) {
    UNUSED(regs);

    return 1713812590; // Arbitrary time (2024-04-22 12:03:10)
}

uint64_t syscall_sigprocmask(regs_t *regs) {
    return (uint64_t)ksigprocmask(regs->rdi, (const sigset_t *)regs->rsi, (sigset_t *)regs->rdx);
}

uint64_t syscall_fcntl(regs_t *regs) {
    return (uint64_t)kfcntl(regs->rdi, regs->rsi, regs->rdx);
}

uint64_t syscall_getuid(regs_t *regs) {
    UNUSED(regs);

    return current_process->uid;
}

uint64_t syscall_getgid(regs_t *regs) {
    UNUSED(regs);

    return current_process->gid;
}

uint64_t syscall_geteuid(regs_t *regs) {
    UNUSED(regs);

    return current_process->euid;
}

uint64_t syscall_setpgid(regs_t *regs) {
    current_process->gid = regs->rdi;
    return 0;
}

uint64_t syscall_setuid(regs_t *regs) {
    current_process->uid = regs->rdi;
    return 0;
}

uint64_t syscall_syslog(regs_t *regs) {
    // first arg is type, second is message
    serial_printf("SYSLOG TYPE %d\n", regs->rdi);
    // if the message is a pointer, print it
    if (regs->rsi) {
        serial_printf("SYSLOG: %s\n", (char *)regs->rsi);
    }
    return 0;
}

uint64_t syscall_select(regs_t *regs) {
    return (uint64_t)kselect(regs->rdi, (fd_set *)regs->rsi, (fd_set *)regs->rdx, (fd_set *)regs->r10, (struct timeval *)regs->r8);
}

void syscall_init() {
    for (int i = 0; i < 512; i++) {
        syscall_table[i] = NULL;
    }

    syscall_table[0] = &syscall_read;
    syscall_table[1] = &syscall_write;
    syscall_table[2] = &syscall_open;
    syscall_table[3] = &syscall_close;
    syscall_table[4] = &syscall_stat;
    syscall_table[5] = &syscall_fstat;
    syscall_table[8] = &syscall_lseek;
    syscall_table[12] = &syscall_brk;
    syscall_table[13] = &syscall_rt_sigaction;
    syscall_table[14] = &syscall_sigprocmask;
    syscall_table[15] = &syscall_rt_sigret;
    syscall_table[16] = &syscall_ioctl;
    syscall_table[23] = &syscall_select;
    syscall_table[39] = &syscall_getpid;
    syscall_table[57] = &syscall_fork;
    syscall_table[59] = &syscall_execv;
    syscall_table[60] = &syscall_exit;
    syscall_table[61] = &syscall_wait4;
    syscall_table[62] = &syscall_kill;
    syscall_table[72] = &syscall_fcntl;
    syscall_table[79] = &syscall_getcwd;
    syscall_table[80] = &syscall_chdir;
    syscall_table[96] = &syscall_timeofday;
    syscall_table[102] = &syscall_getuid;
    syscall_table[103] = &syscall_syslog;
    syscall_table[104] = &syscall_getgid;
    syscall_table[105] = &syscall_setuid;
    syscall_table[107] = &syscall_geteuid;
    syscall_table[109] = &syscall_setpgid;
    syscall_table[110] = &syscall_getppid;
    syscall_table[111] = &syscall_getpgrp;
    syscall_table[217] = &getdents64;
}


extern uint64_t syscall_old_rsp;
extern uint8_t syscall_stack_top;
extern uint8_t syscall_stack;
extern xmm_regs_t xmm_regs;
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
    current_process->syscall_xmm_registers = xmm_regs;
    if (regs->rsp < VIRT_MEM_OFFSET) {
        current_process->user_rsp = regs->rsp;
    }

    if (syscall_table[regs->rax] != NULL) {
        current_process->in_syscall = true;
        uint64_t raxval = syscall_table[regs->rax]((regs_t *)&(current_process->syscall_registers));
        current_process->syscall_registers.rax = raxval;
        current_process->in_syscall = false;
    } else {
        serial_printf("Unknown syscall: %d\n", regs->rax);
        regs_dump((regs_t *)&current_process->syscall_registers);
        union wait status;
        status.w_T.w_Retcode = 0; // Terminated
        status.w_T.w_Coredump = false;
        status.w_T.w_Termsig = SIGSYS;
        process_exit_abnormal(status);
    }

    syscall_old_rsp = current_process->syscall_rsp;

    check_signals(true);

    xmm_regs = current_process->syscall_xmm_registers;

    // move the address of the current process registers to rax
    asm volatile("mov %0, %%rax" ::"r"(&current_process->syscall_registers));
    // jump to after_syscall
    asm volatile("jmp after_syscall");

    while (1)
        ;
}