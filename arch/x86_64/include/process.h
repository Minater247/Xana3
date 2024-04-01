#ifndef _PROCESS_H
#define _PROCESS_H

#include <stdint.h>
#include <stddef.h>

#include <sys/types.h>
#include <memory.h>
#include <system.h>

#define TASK_RUNNING 0
#define TASK_STOPPED 1
#define TASK_INITIAL 2
#define TASK_FORKED 3

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
} exit_status_bits_t;


typedef struct process {
    pid_t pid;
    gid_t pgid;
    page_directory_t *pml4;

    uint32_t status;

    void *entry;

    uint64_t rsp, rbp;

    struct process *next;
    struct process *queue_next;
} process_t;

process_t *create_process(void *entry, uint64_t stack_size, page_directory_t *pml4, bool has_stack);
void schedule();
void process_init();
void add_process(process_t *process);
int64_t fork();
int64_t execv(regs_t *regs);

extern process_t *current_process;

#endif