#include <stdint.h>
#include <stddef.h>

#include <process.h>
#include <memory.h>
#include <system.h>
#include <io.h>
#include <tables.h>
#include <unused.h>
#include <errors.h>
#include <sys/errno.h>
#include <filesystem.h>
#include <elf_loader.h>

#include <display.h>
#include <serial.h>

process_t *process_list = NULL;

process_t *queue = NULL;

process_t *current_process = NULL;

process_t idle_process;

pid_t first_free_pid() {
    pid_t pid = 0;
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

/**
 * Create a new process.
 * 
 * @param entry The entry point of the process
 * @param stack_size The size of the stack, or the address of the stack if has_stack is true
 * @param pml4 The page directory for the process
 * @param has_stack Whether the stack is already set up
*/
process_t *create_process(void *entry, uint64_t stack_size, page_directory_t *pml4, bool has_stack) {
    pid_t pid = first_free_pid();

    process_t *new_process = (process_t *)kmalloc(sizeof(process_t));
    new_process->pid = pid;
    new_process->entry = entry;
    new_process->pml4 = pml4;
    new_process->status = TASK_INITIAL;
    new_process->queue_next = NULL;
    new_process->next = NULL;

    if (!has_stack) {
        // Set up the stack
        uint32_t stack_size_pages = stack_size & 0xFFF ? (stack_size >> 12) + 1 : stack_size >> 12;
        for (uint32_t i = 0; i < stack_size_pages; i++) {
            uint64_t phys = first_free_page_addr();
            map_page_kmalloc(VIRT_MEM_OFFSET - ((i + 1) * 0x1000), phys, false, true, pml4);
        }

        new_process->rsp = VIRT_MEM_OFFSET;
        new_process->rbp = VIRT_MEM_OFFSET;
    } else {
        new_process->rsp = stack_size;
        new_process->rbp = stack_size;
    }

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
    new_process->queue_next = NULL;

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
    } else if (new_process->status == TASK_RUNNING) {
        // Change page directory
        asm volatile ("mov %0, %%cr3" : : "r" (new_process->pml4->phys_addr));
        current_pml4 = new_process->pml4;

        // switch to the new process's stack
        asm volatile ("movq %0, %%rsp" : : "r" (new_process->rsp));
        asm volatile ("movq %0, %%rbp" : : "r" (new_process->rbp));

        return;
    } else if (new_process->status == TASK_FORKED) {
        // Change page directory
        asm volatile ("mov %0, %%cr3" : : "r" (new_process->pml4->phys_addr));
        current_pml4 = new_process->pml4;

        // switch to the new process's stack
        asm volatile ("movq %0, %%rsp" : : "r" (new_process->rsp));
        asm volatile ("movq %0, %%rbp" : : "r" (new_process->rbp));

        new_process->status = TASK_RUNNING;

        // zero out rax
        asm volatile ("movq $0, %rax");

        // jump to the new process
        asm volatile ("jmp *%0" : : "r" (new_process->entry) : "rax");

        while (true);

        return;
    }
}

extern uint64_t read_rip();

int64_t fork() {
    page_directory_t *new_pml4 = clone_page_directory(current_pml4);

    uint64_t rsp, rbp;
    asm volatile ("movq %%rsp, %0;" : "=r" (rsp));
    asm volatile ("movq %%rbp, %0;" : "=r" (rbp));

    process_t *new_process = create_process(0, 0, new_pml4, true);
    new_process->status = TASK_FORKED;
    new_process->queue_next = NULL;
    new_process->rsp = rsp;
    new_process->rbp = rbp;

    add_process(new_process);

    uint64_t rip = read_rip();

    if (rip == 0) {
        return 0;
    } else {
        new_process->entry = (void *)rip;
    }

    return new_process->pid;
}

int64_t execv(regs_t *regs) {
    printf("execv:\n");
    printf("\tPath: %s\n", regs->rdi);
    printf("\tArgv: %lx\n", regs->rsi);
    printf("\tEnvp: %lx\n", regs->rdx);

    kassert(regs->rdx == regs->rsi);

    printf("Args:\n");
    char **argv = (char **)regs->rsi;
    while (*argv != NULL) {
        printf("\t%s\n", *argv);
        argv++;
    }
    printf("argc: %d\n", argv - (char **)regs->rsi);

    printf("Envs:\n");
    char **envp = (char **)regs->rdx;
    while (*envp != NULL) {
        printf("\t%s\n", *envp);
        envp++;
    }
    printf("envc: %d\n", envp - (char **)regs->rdx);

    int fd = fopen((char *)regs->rdi, 0, 0);
    if (fd < 0) {
        printf("Failed to open file\n");
        return -ENOENT;
    }

    size_t size = file_size_internal((char *)regs->rdi);
    // if the highest bit is set, it's an error
    if (size & 0x8000000000000000) {
        printf("Failed to get file size\n");
        return -EIO;
    }

    char *buf = kmalloc(size);
    int read = fread(buf, 1, size, fd);
    if (read < 0) {
        printf("Failed to read file\n");
        return -EIO;
    }

    printf("Read %d bytes\n", read);

    // For now it should suffice to just set up the page directory and jump to the new process
    // In the future, we need to read args/env before messing with pages
    page_directory_t *old_pml4 = current_pml4;
    switch_page_directory(kernel_pml4);
    free_page_directory(old_pml4);

    switch_page_directory(clone_page_directory(kernel_pml4));

    uint64_t entry = load_elf64(buf);
    fclose(fd);
    kfree(buf);

    for (uint32_t i = 0; i < 0x10000; i += 0x1000) {
        map_page_kmalloc(VIRT_MEM_OFFSET - i, first_free_page_addr(), false, true, current_pml4);
    }

    // RBP not set up, do now
    asm volatile ("movq %0, %%rbp" : : "r" (VIRT_MEM_OFFSET));

    // jump to the new process
    jump_to_usermode(entry, VIRT_MEM_OFFSET);

    while (1);
}