#ifndef _PROCESS_H
#define _PROCESS_H

#include <stdint.h>
#include <stddef.h>

#include <sys/types.h>
#include <memory.h>
#include <system.h>
#include <filesystem.h>

#define PROCESS_INITIAL_STACK 0x10000
#define MAX_STACK_SIZE 0x100000

#define TASK_RUNNING 0
#define TASK_STOPPED 1
#define TASK_INITIAL 2
#define TASK_FORKED 3
#define TASK_EXITED 4
#define TASK_WAITING 5

#define WNOHANG 0b1;
#define WCONTINUED 0b10;

/*

Exit Status Code:
Bits 0-7:   Least significant 8 bits of exit() status
Bits 8-13:  Signal which caused process termination (if WIFSIGNALED)
Bits 14-19: Signal which caused process STOP (if WIFSTOPPED)
Bit 20: Whether the child terminated normally (exit())
Bit 21: Whether the child was terminated via signal
Bit 22: Whether the child produced a core dump
Bit 23: Whether the child has stopped via signal
Bit 24: Whether the child was continued via SIGCONT

*/
typedef struct exit_status_bits {
    uint8_t exit_status : 8;  // Bits 0-7
    uint16_t term_signal : 6;  // Bits 8-13
    uint16_t stop_signal : 6;  // Bits 14-19
    uint16_t normal_exit : 1;  // Bit 20
    uint16_t signal_term : 1;  // Bit 21
    uint16_t core_dump : 1;    // Bit 22
    uint16_t signal_stop : 1;  // Bit 23
    uint16_t continued : 1;    // Bit 24
    uint16_t has_terminated : 1; // Bit 25
} exit_status_bits_t;

#define WIFEXITED(status) (((exit_status_bits_t)status).normal_exit)
#define WEXITSTATUS(status) (((exit_status_bits_t)status).exit_status)
#define WIFSIGNALED(status) (((exit_status_bits_t)status).signal_term)
#define WTERMSIG(status) (((exit_status_bits_t)status).term_signal)
#define WCOREDUMP(status) (((exit_status_bits_t)status).core_dump)
#define WIFSTOPPED(status) (((exit_status_bits_t)status).signal_stop)
#define WSTOPSIG(status) (((exit_status_bits_t)status).stop_signal)
#define WIFCONTINUED(status) (((exit_status_bits_t)status).continued)\
// custom bit
#define WIFTERMINATED(status) (((exit_status_bits_t)status).has_terminated)

#define WAIT_PID 0

#define SIGSEGV 11
#define SIGSYS 12
#define SIG_MAX 32

typedef struct signal {
    regs_t registers; // return context for when the signal is handled

    int signal_number;
    int signal_error;
    int signal_code;

    pid_t sender_pid;
    uid_t sender_uid;

    int exit_status;

    void *fault_address;

    regs_t syscall_registers;
    regs_t interrupt_registers;
    void *syscall_stack;
    void *tss_stack;
    uint64_t syscall_rsp;

    struct signal *next;
} signal_t;

typedef uint64_t sigset_t;

struct sigaction {
    void (*signal_handler)(int, signal_t *, void *);
    sigset_t sa_mask;
    int sa_flags;
} __attribute__((packed));

typedef struct process {
    pid_t pid;
    gid_t pgid;
    page_directory_t *pml4;

    uint32_t status;

    void *entry;

    uint64_t rsp, rbp;

    exit_status_bits_t exit_status;

    void *tss_stack;
    uint64_t syscall_rsp;
    void *syscall_stack;
    bool in_syscall;
    bool in_signal_handler;

    regs_t syscall_registers;
    regs_t interrupt_registers;

    file_descriptor_t *file_descriptors;
    char pwd[PATH_MAX];

    signal_t *queued_signals;
    struct sigaction signal_handlers[SIG_MAX];

    uint64_t stack_low;
    uint64_t brk_start;

    struct process *next;
    struct process *queue_next;
} process_t;

process_t *create_process(void *entry, uint64_t stack_size, page_directory_t *pml4, bool has_stack);
void schedule();
void process_init();
void add_process(process_t *process);
int64_t fork();
int64_t execv();
void process_exit(int status);
int64_t process_wait(pid_t pid, void *status, int options, void *rstatus);
void process_exit_abnormal(exit_status_bits_t status);
uint64_t brk(uint64_t increment);
int64_t rt_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);

extern volatile process_t *current_process;

#endif