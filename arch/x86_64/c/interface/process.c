#include <stdint.h>
#include <stddef.h>

#include <process.h>
#include <display.h>
#include <memory.h>
#include <system.h>
#include <serial.h>
#include <io.h>
#include <tables.h>

process_t *process_list = NULL;

process_t *queue = NULL;

process_t *current_process = NULL;

process_t idle_process;

pid_t first_free_pid() {
    pid_t pid = 1;
    if (process_list == NULL) {
        return pid;
    }

    process_t *current = process_list;
    while (current != NULL) {
        if (current->pid == pid) {
            pid++;
        } else {
            return pid;
        }
        current = current->next;
    }

    return pid;
}

void add_process(process_t *process) {
    if (process_list == NULL) {
        process_list = process;
        return;
    }

    // sort by pid
    process_t *current = process_list;
    while (current->next != NULL) {
        current = current->next;
        if (current->pid > process->pid) {
            process->next = current->next;
            current->next = process;
            return;
        }
    }
    current->next = process;

    // add to the queue
    if (queue == NULL) {
        queue = process;
    } else {
        process_t *current = queue;
        while (current->queue_next != NULL) {
            current = current->queue_next;
        }
        current->queue_next = process;
    }
}

process_t *create_process(void *entry, size_t stack_size, page_directory_t *pml4) {
    pid_t pid = first_free_pid();

    serial_printf("Creating process: entry=0x%x, stack_size=%d\n", entry, stack_size);
    serial_printf("PID: %d\n", pid);

    process_t *new_process = (process_t *)kmalloc(sizeof(process_t));
    new_process->pid = pid;
    new_process->entry = entry;
    new_process->pml4 = pml4;
    new_process->status = TASK_INITIAL;
    new_process->next = NULL;

    // Set up the stack
    uint32_t stack_size_pages = stack_size & 0xFFF ? (stack_size >> 12) + 1 : stack_size >> 12;
    for (uint32_t i = 0; i < stack_size_pages; i++) {
        uint64_t phys = first_free_page_addr();
        map_page_kmalloc(VIRT_MEM_OFFSET - ((i + 1) * 0x1000), phys, false, true, pml4);
    }

    new_process->rsp = VIRT_MEM_OFFSET;
    new_process->rbp = VIRT_MEM_OFFSET;

    return new_process;
}

void process_init() {
    // create the idle process
    idle_process.pid = 0;
    idle_process.pml4 = current_pml4;
    idle_process.entry = NULL;
    idle_process.next = NULL;
    idle_process.status = TASK_RUNNING;

    current_process = &idle_process;
    process_list = &idle_process;
}

void schedule() {
    serial_printf("TASK SWITCH: %d\n", current_process->pid);
    serial_printf("NEXT ON QUEUE: %d\n", queue == NULL ? -1 : queue->pid);

    // really basic scheduler for now, just switch to the next process in the queue

    if (queue == NULL) {
        return;
    }

    // Save the current process's rsp and rbp
    asm volatile ("movq %%rsp, %0;" : "=r" (current_process->rsp));
    asm volatile ("movq %%rbp, %0;" : "=r" (current_process->rbp));

    process_t *new_process = queue;
    queue = queue->queue_next;
    // add the current process back to the queue
    if (queue == NULL) {
        queue = current_process;
    } else {
        process_t *current = queue;
        while (current->queue_next != NULL) {
            current = current->queue_next;
        }
        current->queue_next = current_process;
    }
    current_process = new_process;

    serial_printf("Switching to process: %d\n", new_process->pid);

    if (new_process->status == TASK_INITIAL) {
        new_process->status = TASK_RUNNING;
        asm volatile ("mov %0, %%cr3" : : "r" (new_process->pml4->phys_addr));
        current_pml4 = new_process->pml4;

        // switch to the new process's stack
        asm volatile ("movq %0, %%rsp" : : "r" (new_process->rsp));
        asm volatile ("movq %0, %%rbp" : : "r" (new_process->rbp));
        asm volatile ("sti");

        // jump to the new process
        jump_to_usermode((uint64_t)new_process->entry, new_process->rsp);
    } else {
        // switch to the new process's stack
        asm volatile ("movq %0, %%rsp" : : "r" (new_process->rsp));
        asm volatile ("movq %0, %%rbp" : : "r" (new_process->rbp));

        asm volatile ("xchg %bx, %bx");

        return;
    }
}