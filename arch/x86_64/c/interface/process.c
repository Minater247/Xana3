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

pid_t first_free_pid()
{
    pid_t pid = 0;
    if (process_list == NULL)
    {
        return pid;
    }

    process_t *current = process_list;
    while (current != NULL)
    {
        if (current->pid == pid)
        {
            pid++;
        }
        else
        {
            return pid;
        }
        current = current->next;
    }

    return pid;
}

void add_process(process_t *process)
{
    if (process_list == NULL)
    {
        process_list = process;
        return;
    }

    // sort by pid
    process_t *current = process_list;
    while (current->next != NULL)
    {
        current = current->next;
        if (current->pid > process->pid)
        {
            process->next = current->next;
            current->next = process;
            return;
        }
    }
    current->next = process;

    // add to the queue
    if (queue == NULL)
    {
        queue = process;
    }
    else
    {
        process_t *current = queue;
        while (current->queue_next != NULL)
        {
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
process_t *create_process(void *entry, uint64_t stack_size, page_directory_t *pml4, bool has_stack)
{
    pid_t pid = first_free_pid();

    process_t *new_process = (process_t *)kmalloc(sizeof(process_t));
    new_process->pid = pid;
    new_process->entry = entry;
    new_process->pml4 = pml4;
    new_process->status = TASK_INITIAL;
    new_process->queue_next = NULL;
    new_process->next = NULL;
    new_process->tss_stack = kmalloc(4096);
    new_process->syscall_stack = kmalloc(4096); // TODO: free this somewhere
    new_process->exit_status.normal_exit = false;
    new_process->exit_status.exit_status = 0;

    if (!has_stack)
    {
        // Set up the stack
        uint32_t stack_size_pages = stack_size & 0xFFF ? (stack_size >> 12) + 1 : stack_size >> 12;
        for (uint32_t i = 0; i < stack_size_pages; i++)
        {
            uint64_t phys = first_free_page_addr();
            map_page_kmalloc(VIRT_MEM_OFFSET - ((i + 1) * 0x1000), phys, false, true, pml4);
        }

        new_process->rsp = VIRT_MEM_OFFSET;
        new_process->rbp = VIRT_MEM_OFFSET;
    }
    else
    {
        new_process->rsp = stack_size;
        new_process->rbp = stack_size;
    }

    return new_process;
}

uint32_t num_2_schedules = 0;

void process_init()
{
    // create the idle process
    idle_process.pid = 0;
    idle_process.pml4 = current_pml4;
    idle_process.entry = NULL;
    idle_process.next = NULL;
    idle_process.status = TASK_RUNNING;
    idle_process.queue_next = NULL;
    idle_process.tss_stack = kmalloc(4096);
    idle_process.syscall_stack = kmalloc(4096);

    current_process = &idle_process;
    process_list = &idle_process;

    num_2_schedules = 0;
}

void schedule()
{
    // really basic scheduler for now, just switch to the next process in the queue
    if (queue == NULL)
    {
        return;
    }
    ASM_DISABLE_INTERRUPTS;

    // Save the current process's rsp and rbp
    ASM_READ_RSP(current_process->rsp);
    ASM_READ_RBP(current_process->rbp);

    process_t *new_process = queue;
    queue = queue->queue_next;
    // add the current process back to the queue
    if (current_process->status != TASK_EXITED && current_process->status != TASK_WAITING)
    {
        if (queue == NULL)
        {
            queue = current_process;
        }
        else
        {
            process_t *current = queue;
            while (current->queue_next != NULL)
            {
                current = current->queue_next;
            }
            current->queue_next = current_process;
        }
    }
    current_process = new_process;
    new_process->queue_next = NULL;

    tss_set_rsp0((uint64_t)new_process->tss_stack + 4096);

    if (new_process->status == TASK_INITIAL)
    {
        new_process->status = TASK_RUNNING;

        ASM_SET_CR3(new_process->pml4->phys_addr);
        current_pml4 = new_process->pml4;

        // switch to the new process's stack
        ASM_WRITE_RSP(new_process->rsp);
        ASM_WRITE_RBP(new_process->rbp);

        // jump to the new process
        jump_to_usermode((uint64_t)new_process->entry, new_process->rsp);
    }
    else if (new_process->status == TASK_RUNNING)
    {
        ASM_SET_CR3(new_process->pml4->phys_addr);
        current_pml4 = new_process->pml4;

        // switch to the new process's stack
        ASM_WRITE_RSP(new_process->rsp);
        ASM_WRITE_RBP(new_process->rbp);

        // *should* just pop and return to the assembly handler, not
        // touching variables or registers. Leaving this be until I can
        // either confirm this works or find a better way to do it.

        return;
    }
    else if (new_process->status == TASK_FORKED)
    {

        ASM_SET_CR3(new_process->pml4->phys_addr);
        current_pml4 = new_process->pml4;

        new_process->status = TASK_RUNNING;

        // switch to the new process's stack
        ASM_WRITE_RSP(new_process->rsp);
        ASM_WRITE_RBP(new_process->rbp);

        // zero out rax
        asm volatile("movq $0, %rax");

        // jump to the new process
        asm volatile("jmp *%0" : : "r"(new_process->entry) : "rax");

        while (true)
            ;

        return;
    }
}

char fork_stack[0x1000];

void process_exit(int status)
{
    // switch to the fork stack since we're back to the kernel directory
    // note: each process should have its own syscall stack! do not do this
    // TODO: fix this
    asm volatile("movq %0, %%rsp" : : "r"((uint64_t)&fork_stack + 0x1000));
    asm volatile("movq %0, %%rbp" : : "r"((uint64_t)&fork_stack + 0x1000));

    current_process->status = TASK_EXITED;

    // free the process's memory
    kfree(current_process->tss_stack);

    switch_page_directory(kernel_pml4);
    free_page_directory(current_process->pml4);

    // Update the exit status and waiters
    current_process->exit_status.normal_exit = true;
    current_process->exit_status.exit_status = status;
    current_process->exit_status.has_terminated = true;

    schedule();
}

int64_t process_wait(int wait_type, pid_t pid, void *status, int options)
{
    UNUSED(options);
    UNUSED(wait_type);

    // Locate the process we're waiting on
    process_t *current = process_list;
    while (current != NULL)
    {
        if (current->pid == pid)
        {
            break;
        }
        current = current->next;
    }
    if (current == NULL)
    {
        return -ECHILD;
    }

    ASM_ENABLE_INTERRUPTS;
    while (!WIFTERMINATED(current->exit_status)) {
        serial_printf("Waiting for process %d...\n", pid);
        schedule();
    }
    ASM_DISABLE_INTERRUPTS;

    exit_status_bits_t *status_bits = (exit_status_bits_t *)status;
    *status_bits = current->exit_status;

    return pid;
}

extern uint64_t read_rip();

// 0xffffff8000444012

int64_t fork()
{
    uint64_t rsp, rbp;
    ASM_READ_RSP(rsp);
    ASM_READ_RBP(rbp);

    uint64_t rip = read_rip();
    if (rip == 0)
    {
        return 0;
    }

    page_directory_t *new_pml4 = clone_page_directory(current_pml4);

    process_t *new_process = create_process(0, 0, new_pml4, true);
    new_process->status = TASK_FORKED;
    new_process->queue_next = NULL;
    new_process->rsp = rsp;
    new_process->rbp = rbp;
    new_process->entry = (void *)rip;
    new_process->syscall_rsp = current_process->syscall_rsp;

    add_process(new_process);

    return new_process->pid;
}

int64_t execv(regs_t *regs)
{
    // switch to the temporary stack
    ASM_WRITE_RSP((uint64_t)&fork_stack + 0x1000);

    int fd = fopen((char *)regs->rdi, 0, 0);
    if (fd < 0)
    {
        printf("Failed to open file\n");
        return -ENOENT;
    }

    size_t size = file_size_internal((char *)regs->rdi);
    // if the highest bit is set, it's an error
    if (size & 0x8000000000000000)
    {
        printf("Failed to get file size\n");
        return -EIO;
    }

    char *buf = kmalloc(size);
    int read = fread(buf, 1, size, fd);
    if (read < 0)
    {
        printf("Failed to read file\n");
        return -EIO;
    }

    // For now it should suffice to just set up the page directory and jump to the new process
    // In the future, we need to read args/env before messing with pages

    page_directory_t *new_directory = clone_page_directory(kernel_pml4);

    for (uint32_t i = 0; i < 0x10000; i += 0x1000)
    {
        map_page_kmalloc(VIRT_MEM_OFFSET - (i + 0x1000), first_free_page_addr(), false, true, new_directory);
    }

    uint64_t entry = load_elf64(buf, new_directory);
    fclose(fd);
    kfree(buf);

    current_process->pml4 = new_directory;

    ASM_SET_CR3(new_directory->phys_addr);

    free_page_directory(current_pml4);

    current_pml4 = new_directory;

    // jump to the new process
    jump_to_usermode(entry, VIRT_MEM_OFFSET);

    while (1)
        ;
}