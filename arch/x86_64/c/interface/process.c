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
process_t *queue_tail = NULL;
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
        queue_tail = process;
    }
    else
    {
        queue_tail->queue_next = process;
        queue_tail = process;
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
process_t *create_process(void *entry, uint64_t stack_size, page_directory_t *pml4, bool has_stack, memregion_t *regions)
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
    new_process->in_signal_handler = false;
    new_process->queued_signals = NULL;
    // clear the signal handlers
    for (int i = 0; i < SIG_MAX; i++)
    {
        new_process->signal_handlers[i].signal_handler = NULL;
    }

    new_process->file_descriptors = NULL;
    strcpy((char *)new_process->pwd, "/");

    new_process->memory_regions = regions;

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

        // map the region
        memregion_t *region = kmalloc(sizeof(memregion_t));
        region->start = VIRT_MEM_OFFSET - MAX_STACK_SIZE;
        region->end = VIRT_MEM_OFFSET - 1;
        region->flags = 0x7;
        region->next = regions;
        new_process->memory_regions = region;
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

    signal->next = current->queued_signals;
    current->queued_signals = signal;

    signal->syscall_stack = NULL;
    signal->handled = false;
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

int ksigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    UNUSED(how);

    if (oldset != NULL)
    {
        *oldset = current_process->signal_mask;
    }

    if (set != NULL)
    {
        current_process->signal_mask = *set;
    }

    // how is used to modify the signal mask, but we don't support that yet

    return 0;
}

extern uint64_t syscall_old_rsp;
extern void run_signal(uint64_t rsp, void *handler, int signal, signal_t *signal_info, void *context);
void check_signals(bool is_after_syscall) {
    if (current_process->queued_signals) {
        if (!current_process->queued_signals->handled) {
            // Check for special signals
            if (current_process->queued_signals->signal_number == SIGKILL) {
                union wait status;
                status.w_T.w_Retcode = 0;
                status.w_T.w_Coredump = false;
                status.w_T.w_Termsig = SIGKILL;
                process_exit_abnormal(status);
            }

            if (!current_process->signal_handlers[current_process->queued_signals->signal_number].signal_handler ||
                !is_mapped_user((uint64_t)current_process->signal_handlers[current_process->queued_signals->signal_number].signal_handler, current_process->pml4)) {
                    
                // should we ignore this?
                int signo = current_process->queued_signals->signal_number;
                if (signo == SIGCHLD || signo == SIGURG || signo == SIGWINCH) {
                    signal_t *signal = current_process->queued_signals;
                    current_process->queued_signals = signal->next;
                    kfree(signal);
                    return;
                }

                // no, we shouldn't
                union wait status;
                status.w_T.w_Retcode = 0;
                status.w_T.w_Coredump = false;
                status.w_T.w_Termsig = signo;
                process_exit_abnormal(status);
            }

            signal_t *signal = current_process->queued_signals;

            signal->interrupt_registers = current_process->interrupt_registers;
            signal->syscall_registers = current_process->syscall_registers;

            signal->syscall_stack = (void *)kmalloc(SYSCALL_STACK_SIZE);
            memcpy(signal->syscall_stack, current_process->syscall_stack, SYSCALL_STACK_SIZE);
            signal->syscall_rsp = current_process->syscall_rsp;

            signal->handled = true;

            current_process->in_signal_handler = true;

            signal->was_in_syscall = current_process->in_syscall;

            uint64_t rsp = is_after_syscall ? syscall_old_rsp : (current_process->interrupt_registers.rsp >= VIRT_MEM_OFFSET ? current_process->user_rsp : current_process->interrupt_registers.rsp);

            if (virt_to_phys(rsp, current_pml4) == (uint64_t)-1) {
                union wait status;
                status.w_T.w_Retcode = 0;
                status.w_T.w_Coredump = false;
                status.w_T.w_Termsig = SIGSEGV;
                process_exit_abnormal(status);
            }

            run_signal(rsp, current_process->signal_handlers[signal->signal_number].signal_handler, signal->signal_number, signal, NULL);
        }
    }
}

extern tss_entry_t tss;
extern xmm_regs_t xmm_regs;
void schedule()
{
    ASM_DISABLE_INTERRUPTS; // In case we aren't called from an interrupt

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
                queue_tail->queue_next = (process_t *)current_process;
                queue_tail = (process_t *)current_process;
            }
        }
        current_process = new_process;
        current_process->queue_next = NULL;

        tss.rsp0 = (uint64_t)current_process->tss_stack + SYSCALL_STACK_SIZE;

        ASM_SET_CR3(current_process->pml4->phys_addr);
        current_pml4 = current_process->pml4;
    }

    check_signals(false);

    if (current_process->status == TASK_INITIAL)
    {
        current_process->status = TASK_RUNNING;

        // jump to the new process
        jump_to_usermode((uint64_t)current_process->entry, current_process->rsp, 0, NULL, NULL);
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
        current_process->status = TASK_RUNNING;

        // zero out rax
        current_process->syscall_registers.rax = 0;

        syscall_old_rsp = current_process->syscall_rsp;

        xmm_regs = current_process->syscall_xmm_registers;

        // move the address of the current process registers to rax
        asm volatile("mov %0, %%rax" ::"r"(&(current_process->syscall_registers)));
        // jump to after_syscall
        asm volatile("jmp after_syscall");
    }

    kpanic("Process %d in undefined state! [%d]", current_process->pid, current_process->status);
}

char temp_stack[SYSCALL_STACK_SIZE];

int64_t krt_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    UNUSED(oldact);

    if (signum >= SIG_MAX)
    {
        return -EINVAL;
    }

    if (oldact != NULL)
    {
        *oldact = current_process->signal_handlers[signum];
    }

    if (act != NULL)
    {
        current_process->signal_handlers[signum] = *act;
    }

    return 0;
}

void krt_sigret() {
    // switch to the sigret stack temporarily
    ASM_WRITE_RSP((uint64_t)temp_stack + SYSCALL_STACK_SIZE);

    current_process->in_signal_handler = false;

    signal_t *signal = current_process->queued_signals;

    memcpy(current_process->syscall_stack, signal->syscall_stack, SYSCALL_STACK_SIZE);
    kfree(signal->syscall_stack);

    current_process->syscall_rsp = signal->syscall_rsp;
    current_process->interrupt_registers = signal->interrupt_registers;
    current_process->syscall_registers = signal->syscall_registers;

    bool was_in_syscall = signal->was_in_syscall;

    signal_t *next = signal->next;
    kfree(signal);
    current_process->queued_signals = next;

    if (!was_in_syscall) {
        schedule();
    } else {
        syscall_old_rsp = current_process->syscall_rsp;

        xmm_regs = current_process->syscall_xmm_registers;

        // move the address of the current process registers to rax
        asm volatile("mov %0, %%rax" ::"r"(&(current_process->syscall_registers)));

        // jump to after_syscall
        asm volatile("jmp after_syscall");
    }
}

void process_exit(int status)
{
    // switch to the temporary stack
    ASM_WRITE_RSP((uint64_t)temp_stack + SYSCALL_STACK_SIZE);

    current_process->status = TASK_EXITED;

    // free the process's memory
    switch_page_directory(kernel_pml4);
    free_page_directory(current_process->pml4);

    // Update the exit status and waiters
    current_process->exit_status.w_T.w_Retcode = status;
    current_process->exit_status.w_T.w_Coredump = 0;
    current_process->exit_status.w_T.w_Termsig = 0;

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
    kfree(current_process->tss_stack);

    IRQ0;
}

void process_exit_abnormal(union wait status)
{
    // switch to the temporary stack
    ASM_WRITE_RSP((uint64_t)temp_stack + SYSCALL_STACK_SIZE);
    
    current_process->status = TASK_EXITED;

    serial_printf("Process %d exited abnormally with status %d\n", current_process->pid, status.w_T.w_Termsig);

    // free the process's memory
    switch_page_directory(kernel_pml4);
    free_page_directory(current_process->pml4);

    // Update the exit status and waiters
    current_process->exit_status = status;
    current_process->status = TASK_EXITED;

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
    kfree(current_process->tss_stack);

    IRQ0;
}

int64_t process_wait(pid_t pid, void *status, int options, void *rusage)
{
    UNUSED(options);
    UNUSED(rusage);

    int *status_int = (int *)status;

    // if it wants to wait on itself, return an error
    if (pid == current_process->pid)
    {
        serial_printf("Attempted to wait on self\n");
        *status_int = 0;
        return -ECHILD;
    }
    
    process_t *current = process_list;
    if (pid == -1) {
        // TODO: actually wait for any child
        // for now, just waiting on first child
        while (current != NULL)
        {
            if (current->ppid == current_process->pid)
            {
                break;
            }
            current = current->next;
        }
    } else {
        // Locate the process we're waiting on
        while (current != NULL)
        {
            if (current->pid == pid)
            {
                break;
            }
            current = current->next;
        }
    }
    if (current == NULL)
    {
        serial_printf("Process %d not found\n", pid);
        *status_int = 0;
        return -ECHILD;
    }

    ASM_ENABLE_INTERRUPTS;
    while (!(current->status == TASK_EXITED)) {
        IRQ0;
    }
    ASM_DISABLE_INTERRUPTS;
    
    union wait *status_bits = (union wait *)status;
    if (status_bits != NULL)
    {
        *status_bits = current->exit_status;
    }

    // and done! Remove from the process list, and free the memory
    process_t *prev = process_list;
    if (prev == current) {
        process_list = current->next;
    } else {
        while (prev->next != current) {
            prev = prev->next;
        }
        prev->next = current->next;
    }

    kfree(current);
    
    if (pid != -1) {
        serial_printf("Returning process %d\n", pid);
        return pid;
    } else {
        serial_printf("Returning process %d\n", current->pid);
        return current->pid;
    }
}

extern uint64_t read_rip();

// 0xffffff8000444012

int64_t kfork()
{
    uint64_t rsp, rbp;
    ASM_READ_RSP(rsp);
    ASM_READ_RBP(rbp);

    uint64_t rip = read_rip();
    if (rip == 0)
    {
    }

    page_directory_t *new_pml4 = clone_page_directory(current_pml4);

    // Copy the memregions
    memregion_t *current_old = current_process->memory_regions;
    memregion_t *new_regions = NULL;
    memregion_t *new_current = NULL;
    while (current_old != NULL)
    {
        memregion_t *new_region = kmalloc(sizeof(memregion_t));
        new_region->start = current_old->start;
        new_region->end = current_old->end;
        new_region->flags = current_old->flags;
        new_region->next = NULL;

        if (new_regions == NULL)
        {
            new_regions = new_region;
        }
        else
        {
            new_current->next = new_region;
        }
        new_current = new_region;

        current_old = current_old->next;
    }

    process_t *new_process = create_process(0, 0, new_pml4, true, new_regions);
    new_process->status = TASK_FORKED;
    new_process->queue_next = NULL;
    
    new_process->entry = (void *)rip;
    new_process->syscall_rsp = current_process->syscall_rsp;
    new_process->syscall_registers = current_process->syscall_registers;
    new_process->syscall_xmm_registers = current_process->syscall_xmm_registers;

    new_process->stack_low = current_process->stack_low;

    new_process->queued_signals = NULL;
    new_process->in_signal_handler = false;
    for (int i = 0; i < SIG_MAX; i++)
    {
        new_process->signal_handlers[i].signal_handler = current_process->signal_handlers[i].signal_handler;
    }
    
    new_process->file_descriptors = clone_file_descriptors(current_process->file_descriptors);
    strcpy(new_process->pwd, (const char *)current_process->pwd);

    new_process->ppid = current_process->pid;

    add_process(new_process);

    return new_process->pid;
}

// TODO: replace with something like binfmt for loading
int64_t kexecv(regs_t *regs)
{
    int fd = kfopen((char *)regs->rdi, 0, 0);
    if (fd < 0)
    {
        return -ENOENT;
    }

    size_t size = file_size_internal((char *)regs->rdi);
    // if the highest bit is set, it's an error
    if (size & 0x8000000000000000)
    {
        return -EIO;
    }

    char *buf = kmalloc(size);
    int read = kfread(buf, 1, size, fd);
    if (read < 0)
    {
        return -EIO;
    }

    // get the args
    char **argv = (char **)regs->rsi;
    char **envp = (char **)regs->rdx;


    int argc = 0;
    int envc = 0;
    uint64_t argv_string_size = 0;
    
    if (argv) {
        while (argv[argc] != NULL) {
            argv_string_size += strlen(argv[argc]) + 1;
            argc++;
        }
    }

    if (envp) {
        while (envp[envc] != NULL) {
            argv_string_size += strlen(envp[envc]) + 1;
            envc++;
        }
    }

    char *temp_strings = kmalloc(argv_string_size);
    char *temp_strings_start = temp_strings;
    // copy all the strings over
    if (argv) {
        for (int i = 0; i < argc; i++) {
            strcpy(temp_strings, argv[i]);
            temp_strings += strlen(argv[i]) + 1;
        }
    }
    if (envp) {
        for (int i = 0; i < envc; i++) {
            strcpy(temp_strings, envp[i]);
            temp_strings += strlen(envp[i]) + 1;
        }
    }
    temp_strings = temp_strings_start;


    page_directory_t *new_directory = clone_page_directory(kernel_pml4);

    for (uint32_t i = 0; i < PROCESS_INITIAL_STACK; i += 0x1000)
    {
        map_page_kmalloc(VIRT_MEM_OFFSET - (i + 0x1000), first_free_page_addr(), false, true, new_directory);
        memset((void *)(VIRT_MEM_OFFSET - i), 0, 0x1000);
    }

    current_process->stack_low = VIRT_MEM_OFFSET - PROCESS_INITIAL_STACK;

    elf_info_t info = load_elf64(buf, new_directory);
    
    kfclose(fd);
    kfree(buf);

    if (info.status != 0)
    {
        switch_page_directory(current_pml4);
        free_page_directory(new_directory);
        kprintf("Failed to load ELF\n");
        return -ENOEXEC;
    }

    serial_printf("ELF loaded.\n");

    current_process->pml4 = new_directory;

    current_process->brk_start = PAGE_ALIGN_UP(info.max_addr);

    // clear the signal handlers
    for (int i = 0; i < SIG_MAX; i++)
    {
        current_process->signal_handlers[i].signal_handler = NULL;
    }

    ASM_SET_CR3(new_directory->phys_addr);

    free_page_directory(current_pml4);

    current_pml4 = new_directory;

    // set up the stack
    uint64_t argv_env_ptr_size = (argc + envc + 2) * sizeof(char *);
    uint64_t stack_loc = VIRT_MEM_OFFSET - (argv_env_ptr_size + argv_string_size);

    uint64_t string_write_loc = stack_loc + argv_env_ptr_size;
    uint64_t string_read_loc = (uint64_t)temp_strings;
    char **argv_env_ptr = (char **)stack_loc;

    // copy the strings
    if (argv) {
        for (int i = 0; i < argc; i++) {
            strcpy((char *)string_write_loc, (char *)string_read_loc);
            *(argv_env_ptr++) = (char *)string_write_loc;
            // increment the write location and read location
            string_write_loc += strlen((char *)string_read_loc) + 1;
            string_read_loc += strlen((char *)string_read_loc) + 1;
        }
    }
    *(argv_env_ptr++) = 0;
    if (envp) {
        for (int i = 0; i < envc; i++) {
            strcpy((char *)string_write_loc, (char *)string_read_loc);
            *(argv_env_ptr++) = (char *)string_write_loc;
            // increment the write location and read location
            string_write_loc += strlen((char *)string_read_loc) + 1;
            string_read_loc += strlen((char *)string_read_loc) + 1;
        }
    }
    *(argv_env_ptr++) = 0;

    kfree(temp_strings);

    // jump to the new process
    jump_to_usermode(info.entry, stack_loc, argc, (char **)(stack_loc), (char **)(stack_loc + (argc + 1) * sizeof(char *)));

    while (1)
        ;
}

uint64_t kbrk(uint64_t location) {
    if (location < current_process->brk_start) {
        return current_process->brk_start;
    }

    serial_printf("Adjusting process brk to 0x%lx\n", location);

    location = PAGE_ALIGN_UP(location);

    uint64_t old_brk = current_process->brk_start;
    current_process->brk_start = location;

    // did we cross a page boundary?
    uint64_t old_brk_page = old_brk & 0xFFFFFFFFFFFFF000;
    uint64_t new_brk_page = location & 0xFFFFFFFFFFFFF000;
    if (old_brk_page == new_brk_page) {
        return 0;
    }

    // map the new pages
    memregion_t *new_region = kmalloc(sizeof(memregion_t));
    new_region->start = old_brk_page;
    new_region->end = new_brk_page;
    new_region->flags = 0x7;

    // insert into the list, sorted by start address
    memregion_t *current = current_process->memory_regions;
    memregion_t *prev = NULL;
    while (current != NULL) {
        if (current->start > new_region->start) {
            break;
        }
        prev = current;
        current = current->next;
    }
    // insert before current
    new_region->next = current;
    if (prev == NULL) {
        current_process->memory_regions = new_region;
    } else {
        prev->next = new_region;
    }

    for (uint64_t i = old_brk_page; i <= new_brk_page; i += 0x1000) {
        map_page_kmalloc(i, first_free_page_addr(), false, true, current_process->pml4);
        memset((void *)i, 0, 0x1000);
    }

    return 0;
}

