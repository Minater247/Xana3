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
#define WIFCONTINUED(status) (((exit_status_bits_t)status).continued)
// custom bit
#define WIFTERMINATED(status) (((exit_status_bits_t)status).has_terminated)

#define WAIT_PID 0


#define SIGNULL 0
#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGTRAP 5
#define SIGABRT 6
#define SIGBUS 7
#define SIGFPE 8
#define SIGKILL 9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22
#define SIGURG 23
#define SIGXCPU 24
#define SIGXFSZ 25
#define SIGVTALRM 26
#define SIGPROF 27
#define SIGWINCH 28
#define SIGIO 29
#define SIGPWR 30
#define SIGSYS 31
#define SIG_MAX 31

#define SEGV_MAPERR 1

typedef struct signal {
    regs_t registers; // return context for when the signal is handled

    int signal_number;
    int signal_error;
    int signal_code;

    pid_t sender_pid;
    uid_t sender_uid;

    void *fault_address;

    regs_t syscall_registers;
    regs_t interrupt_registers;
    void *syscall_stack;
    // The TSS stack is only used during interrupts, which should not re-enable interrupts during the handler.
    // As such, at present, it need not be copied during a signal call.
    uint64_t syscall_rsp;

    bool handled;
    bool was_in_syscall;

    struct signal *next;
} signal_t;

typedef unsigned long sigset_t;

struct sigaction {
    void (*signal_handler)(int, signal_t *, void *);
    sigset_t sa_mask;
    int sa_flags;
} __attribute__((packed));

// TODO: Implement memory regions
typedef struct memregion {
    uint64_t start;
    uint64_t end;
    uint64_t flags;
    struct memregion *next;
} memregion_t;

typedef struct process {
    pid_t pid; // Process ID
    gid_t gid; // Group ID
    gid_t pgid; // Parent Group ID
    pid_t ppid; // Parent Process ID
    uid_t uid; // User ID
    uid_t euid; // Effective User ID
    uid_t fsuid; // Filesystem User ID
    uid_t egid; // Effective Group ID
    uid_t fsgid; // Filesystem Group ID
    page_directory_t *pml4;

    uint32_t status;

    void *entry;

    uint64_t rsp, rbp;

    exit_status_bits_t exit_status;

    void *tss_stack;
    uint64_t syscall_rsp;
    void *syscall_stack;
    uint64_t user_rsp;
    bool in_syscall;
    bool in_signal_handler;

    regs_t syscall_registers;
    regs_t interrupt_registers;
    xmm_regs_t syscall_xmm_registers;
    xmm_regs_t interrupt_xmm_registers;

    file_descriptor_t *file_descriptors;
    char pwd[PATH_MAX];

    signal_t *queued_signals;
    struct sigaction signal_handlers[SIG_MAX];

    uint64_t stack_low;
    uint64_t brk_start;

    uint64_t dependents;

    memregion_t *memory_regions;

    struct process *next;
    struct process *queue_next;
} process_t;

process_t *create_process(void *entry, uint64_t stack_size, page_directory_t *pml4, bool has_stack, memregion_t *regions);
void schedule();
void process_init();
void add_process(process_t *process);
int64_t kfork();
int64_t kexecv();
void process_exit(int status);
int64_t process_wait(pid_t pid, void *status, int options, void *rstatus);
void process_exit_abnormal(exit_status_bits_t status);
uint64_t kbrk(uint64_t increment);
int64_t krt_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
void krt_sigret();
void signal_process(pid_t pid, signal_t *signal);
int process_kill(pid_t pid, int signal);
void check_signals(bool is_after_syscall);
int ksigprocmask(int how, const sigset_t *set, sigset_t *oldset);

extern volatile process_t *current_process;

#endif