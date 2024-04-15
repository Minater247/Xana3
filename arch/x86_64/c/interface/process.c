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
#include <syscall.h>
#include <string.h>

#include <display.h>
#include <serial.h>

process_t *process_list = NULL;
process_t *queue = NULL;
volatile process_t *current_process = NULL;

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
    new_process->tss_stack = kmalloc(SYSCALL_STACK_SIZE);
    new_process->syscall_stack = kmalloc(SYSCALL_STACK_SIZE);
    new_process->exit_status.normal_exit = false;
    new_process->exit_status.exit_status = 0;
    new_process->exit_status.has_terminated = false;
    new_process->in_signal_handler = false;
    new_process->queued_signals = NULL;
    // clear the signal handlers
    for (int i = 0; i < SIG_MAX; i++)
    {
        new_process->signal_handlers[i].signal_handler = NULL;
    }

    new_process->file_descriptors = NULL;
    strcpy((char *)new_process->pwd, "/");

    if (!has_stack)
    {
        // Set up the stack
        uint32_t stack_size_pages = stack_size & 0xFFF ? (stack_size >> 12) + 1 : stack_size >> 12;
        for (uint32_t i = 0; i < stack_size_pages; i++)
        {
            uint64_t phys = first_free_page_addr();
            map_page_kmalloc(VIRT_MEM_OFFSET - ((i + 1) * 0x1000), phys, false, true, pml4);
        }

        new_process->stack_low = VIRT_MEM_OFFSET - (stack_size_pages * 0x1000);

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

extern char tss_stack[SYSCALL_STACK_SIZE];
void process_init()
{
    // create the idle process
    idle_process.pid = 0;
    idle_process.pml4 = current_pml4;
    idle_process.entry = NULL;
    idle_process.next = NULL;
    idle_process.status = TASK_RUNNING;
    idle_process.queue_next = NULL;
    idle_process.tss_stack = tss_stack;
    idle_process.syscall_stack = kmalloc(SYSCALL_STACK_SIZE);
    idle_process.exit_status.normal_exit = false;
    idle_process.exit_status.exit_status = 0;
    idle_process.exit_status.has_terminated = false;
    idle_process.queued_signals = NULL;
    idle_process.in_signal_handler = false;
    for (int i = 0; i < SIG_MAX; i++)
    {
        idle_process.signal_handlers[i].signal_handler = NULL;
    }

    current_process = &idle_process;
    process_list = &idle_process;
}

void signal_process(pid_t pid, signal_t *signal)
{
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
        return;
    }

    signal_t *current_signal = current->queued_signals;
    if (current_signal == NULL)
    {
        current->queued_signals = signal;
    }
    else
    {
        while (current_signal->next != NULL)
        {
            current_signal = current_signal->next;
        }
        current_signal->next = signal;
    }

    serial_printf("Sent signal %d to process %d\n", signal->signal_number, pid);

    signal->next = NULL;
    signal->syscall_stack = NULL;
}

int process_kill(pid_t pid, int signal)
{
    if (pid == 0)
    {
        return -EPERM;
    }

    signal_t *signal_info = (signal_t *)kmalloc(sizeof(signal_t));
    signal_info->signal_number = signal;
    signal_info->signal_error = 0;
    signal_info->signal_code = 0;
    signal_info->sender_pid = current_process->pid;
    signal_info->sender_uid = 0;
    signal_info->fault_address = NULL;
    signal_info->next = NULL;

    signal_process(pid, signal_info);

    return 0;
}

signal_t test_signal;

bool min_schedule = false;

extern uint64_t syscall_old_rsp;
extern void run_signal(uint64_t rsp, void *handler, int signal, signal_t *signal_info, void *context);
void schedule()
{
    if (!min_schedule) {
        ASM_DISABLE_INTERRUPTS;
    
        // Save the current process's rsp and rbp
        ASM_READ_RSP(current_process->rsp);
        ASM_READ_RBP(current_process->rbp);

        if (queue != NULL) {

            process_t *new_process = queue;
            queue = queue->queue_next;
            // add the current process back to the queue
            if (current_process->status != TASK_EXITED && current_process->status != TASK_WAITING)
            {
                if (queue == NULL)
                {
                    queue = (process_t *)current_process;
                }
                else
                {
                    process_t *current = queue;
                    while (current->queue_next != NULL)
                    {
                        current = current->queue_next;
                    }
                    current->queue_next = (process_t *)current_process;
                }
            }
            current_process = new_process;
            current_process->queue_next = NULL;

            tss_set_rsp0((uint64_t)current_process->tss_stack + SYSCALL_STACK_SIZE);

            ASM_SET_CR3(current_process->pml4->phys_addr);
            current_pml4 = current_process->pml4;
        }
    } else {
        min_schedule = false;
    }
    
    serial_printf("Scheduling process %d    \n", current_process->pid);

    if (current_process->queued_signals && !current_process->in_signal_handler && !current_process->in_syscall) {
        // Check for special signals
        if (current_process->queued_signals->signal_number == SIGKILL) {
            process_exit_abnormal((exit_status_bits_t){.normal_exit = false, .has_terminated = true, .term_signal = SIGKILL, .exit_status = 0});
        }

        if (!current_process->signal_handlers[current_process->queued_signals->signal_number].signal_handler) {
            kpanic("Process %d has signal %d but no handler!", current_process->pid, current_process->queued_signals->signal_number);
        }

        signal_t *signal = current_process->queued_signals;

        signal->interrupt_registers = current_process->interrupt_registers;
        signal->syscall_registers = current_process->syscall_registers;

        signal->syscall_stack = (void *)kmalloc(SYSCALL_STACK_SIZE);
        memcpy(signal->syscall_stack, current_process->syscall_stack, SYSCALL_STACK_SIZE);
        signal->syscall_rsp = current_process->syscall_rsp;

        serial_printf("Running signal %d on RSP: 0x%lx\n", signal->signal_number, current_process->interrupt_registers.rsp);

        current_process->in_signal_handler = true;

        run_signal(current_process->interrupt_registers.rsp, current_process->signal_handlers[signal->signal_number].signal_handler, signal->signal_number, signal, NULL);
    }

    if (current_process->status == TASK_INITIAL)
    {
        current_process->status = TASK_RUNNING;

        // jump to the new process
        jump_to_usermode((uint64_t)current_process->entry, current_process->rsp);
    }
    else if (current_process->status == TASK_RUNNING)
    {
        // switch to the new process's stack ( see below comment )
        ASM_WRITE_RSP(current_process->rsp);
        ASM_WRITE_RBP(current_process->rbp);

        // *should* just pop and return to the assembly handler, not
        // touching variables or registers. Leaving this be until I can
        // either confirm this works and is limited as such per specifcation,
        // or find a better way to do it.

        return;
    }
    else if (current_process->status == TASK_FORKED)
    {
        process_kill(1, SIGTERM);

        current_process->status = TASK_RUNNING;

        // zero out rax
        current_process->syscall_registers.rax = 0;

        syscall_old_rsp = current_process->syscall_rsp;

        // move the address of the current process registers to rax
        asm volatile("mov %0, %%rax" ::"r"(&(current_process->syscall_registers)));
        // jump to after_syscall
        asm volatile("jmp after_syscall");
    }

    kpanic("Process %d in undefined state! [%d]", current_process->pid, current_process->status);
}

char temp_stack[SYSCALL_STACK_SIZE];

int64_t rt_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    UNUSED(oldact);

    if (signum >= SIG_MAX)
    {
        return -EINVAL;
    }

    // if (oldact != NULL)
    // {
    //     *oldact = current_process->signal_handlers[signum];
    // }

    if (act != NULL)
    {
        current_process->signal_handlers[signum] = *act;
        serial_printf("Set signal %d to 0x%lx, process %d\n", signum, act->signal_handler, current_process->pid);
    }

    return 0;
}

void rt_sigret() {
    // switch to the sigret stack temporarily
    ASM_WRITE_RSP((uint64_t)temp_stack + SYSCALL_STACK_SIZE);

    current_process->in_signal_handler = false;

    memcpy(current_process->syscall_stack, current_process->queued_signals->syscall_stack, SYSCALL_STACK_SIZE);
    kfree(current_process->queued_signals->syscall_stack);

    current_process->syscall_rsp = current_process->queued_signals->syscall_rsp;
    current_process->interrupt_registers = current_process->queued_signals->interrupt_registers;
    current_process->syscall_registers = current_process->queued_signals->syscall_registers;

    signal_t *next = current_process->queued_signals->next;
    // not yet allocated so free would fail
    kfree(current_process->queued_signals);
    current_process->queued_signals = next;

    min_schedule = true;

    schedule();
}

void process_exit(int status)
{
    current_process->status = TASK_EXITED;

    // free the process's memory
    switch_page_directory(kernel_pml4);
    free_page_directory(current_process->pml4);

    // Update the exit status and waiters
    current_process->exit_status.normal_exit = true;
    current_process->exit_status.exit_status = status;
    current_process->exit_status.has_terminated = true;

    // free the signals
    signal_t *current_signal = current_process->queued_signals;
    while (current_signal != NULL)
    {
        signal_t *next = current_signal->next;
        if (current_signal->syscall_stack != NULL)
        {
            serial_printf("Freeing signal syscall stack @ 0x%lx\n", current_signal->syscall_stack);
            kfree(current_signal->syscall_stack);
        }
        serial_printf("Freeing signal @ 0x%lx\n", current_signal);
        kfree(current_signal);
        current_signal = next;
    }

    kfree(current_process->syscall_stack);

    IRQ0;
}

void process_exit_abnormal(exit_status_bits_t status)
{
    current_process->status = TASK_EXITED;

    serial_printf("Process %d exited abnormally with status %d\n", current_process->pid, status.exit_status);

    // free the process's memory
    switch_page_directory(kernel_pml4);
    free_page_directory(current_process->pml4);

    // Update the exit status and waiters
    current_process->exit_status = status;

    // free the signals
    signal_t *current_signal = current_process->queued_signals;
    while (current_signal != NULL)
    {
        signal_t *next = current_signal->next;
        if (current_signal->syscall_stack != NULL)
        {
            kfree(current_signal->syscall_stack);
        }
        kfree(current_signal);
        current_signal = next;
    }

    kfree(current_process->syscall_stack);

    IRQ0;
}

int64_t process_wait(pid_t pid, void *status, int options, void *rusage)
{
    UNUSED(options);
    UNUSED(rusage);

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
        IRQ0;
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
    }

    page_directory_t *new_pml4 = clone_page_directory(current_pml4);

    process_t *new_process = create_process(0, 0, new_pml4, true);
    new_process->status = TASK_FORKED;
    new_process->queue_next = NULL;
    
    new_process->entry = (void *)rip;
    new_process->syscall_rsp = current_process->syscall_rsp;
    new_process->syscall_registers = current_process->syscall_registers;

    new_process->stack_low = current_process->stack_low;

    new_process->queued_signals = NULL;
    
    // TODO: copy file descriptors
    strcpy(new_process->pwd, (const char *)current_process->pwd);

    add_process(new_process);

    return new_process->pid;
}

int64_t execv(regs_t *regs)
{
    serial_printf("execv\n");

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

    for (uint32_t i = 0; i < PROCESS_INITIAL_STACK; i += 0x1000)
    {
        map_page_kmalloc(VIRT_MEM_OFFSET - (i + 0x1000), first_free_page_addr(), false, true, new_directory);
    }

    current_process->stack_low = VIRT_MEM_OFFSET - PROCESS_INITIAL_STACK;

    elf_info_t info = load_elf64(buf, new_directory);
    
    fclose(fd);
    kfree(buf);

    if (info.status != 0)
    {
        free_page_directory(new_directory);
        printf("Failed to load ELF\n");
        return -ENOEXEC;
    }

    current_process->pml4 = new_directory;

    current_process->brk_start = info.max_addr;

    ASM_SET_CR3(new_directory->phys_addr);

    free_page_directory(current_pml4);

    current_pml4 = new_directory;

    serial_printf("Jumping to new process at 0x%lx\n", info.entry);

    // jump to the new process
    jump_to_usermode(info.entry, VIRT_MEM_OFFSET);

    while (1)
        ;
}

uint64_t brk(uint64_t location) {
    if (location < current_process->brk_start) {
        return current_process->brk_start;
    }

    uint64_t old_brk = current_process->brk_start;
    current_process->brk_start = location;

    // did we cross a page boundary?
    uint64_t old_brk_page = old_brk & 0xFFFFFFFFFFFFF000;
    uint64_t new_brk_page = location & 0xFFFFFFFFFFFFF000;
    if (old_brk_page == new_brk_page) {
        return 0;
    }

    // free pages
    uint64_t start = old_brk_page + 0x1000;
    uint64_t end = new_brk_page;
    for (uint64_t i = start; i < end; i += 0x1000) {
        free_page(i, current_process->pml4);
    }

    return 0;
}