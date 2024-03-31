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

typedef struct process {
    pid_t pid;
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