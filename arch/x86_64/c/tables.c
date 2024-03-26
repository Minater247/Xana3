#include <tables.h>
#include <string.h>
#include <io.h>
#include <errors.h>
#include <syscall.h>

extern void load_tss();
extern void load_idt(uint64_t idtr);

uint64_t presyscall_rsp;
uint64_t presyscall_rbp;

gdt_entry_t gdt[7]; // 6 entries, TSS has 2 entries
tss_entry_t tss;
gdt_ptr_t gdtr;

idt_entry_t idt[256];
idtr_t idtr;
isr_handler_t isr_handlers[16];

void gdt_set_entry(int index, uint64_t base, uint64_t limit, uint8_t access, uint8_t granularity, gdt_entry_t *gdt_location)
{
    gdt_location[index].base_low = base & 0xFFFF;
    gdt_location[index].base_middle = (base >> 16) & 0xFF;
    gdt_location[index].base_high = (base >> 24) & 0xFF;

    gdt_location[index].limit_low = limit & 0xFFFF;
    gdt_location[index].granularity = (limit >> 16) & 0x0F;

    gdt_location[index].granularity |= granularity & 0xF0;
    gdt_location[index].access = access;
}

char tss_stack[4096] __attribute__((aligned(16)));

void tss_init()
{
    memset(&tss, 0, sizeof(tss));
    tss.io_map_base = sizeof(tss);

    tss.rsp0 = (uint64_t)tss_stack + sizeof(tss_stack);

    load_tss();
}

extern void load_gdt_initial(uint64_t gdtr);

void gdt_init()
{
    // Null segment
    gdt_set_entry(0, 0, 0, 0, 0, (gdt_entry_t *)&gdt);

    // Code segment
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xA0, (gdt_entry_t *)&gdt);

    // Data segment
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xA0, (gdt_entry_t *)&gdt);

    // TSS entry
    uint64_t tss_base = (uint64_t)&tss;
    uint64_t tss_limit = sizeof(tss);

    tss_64_t *tss_entry = (tss_64_t *)&gdt[3];
    tss_entry->limit_low = tss_limit & 0xFFFF;
    tss_entry->base_low = tss_base & 0xFFFF;
    tss_entry->base_middle = (tss_base >> 16) & 0xFF;
    tss_entry->access = 0xE9;
    tss_entry->limit_flags = ((tss_limit >> 16) & 0xF) | 0x40;
    tss_entry->base_high = (tss_base >> 24) & 0xFF;
    tss_entry->base_upper = (tss_base >> 32) & 0xFFFFFFFF;
    tss_entry->reserved = 0;

    // User mode data segment
    gdt_set_entry(5, 0, 0xFFFFFFFF, 0xF2, 0xA0, (gdt_entry_t *)&gdt);

    // User mode code segment
    gdt_set_entry(6, 0, 0xFFFFFFFF, 0xFA, 0xA0, (gdt_entry_t *)&gdt);

    gdtr.base = (uint64_t)&gdt;
    gdtr.limit = sizeof(gdt) - 1;

    load_gdt_initial((uint64_t)&gdtr);
}

void idt_set_entry(int index, uint64_t handler)
{
    idt[index].offset_low = handler & 0xFFFF;
    idt[index].selector = 0x08;
    idt[index].ist = 0;
    idt[index].type_attr = 0x8E; // Present, ring 0, 64-bit interrupt gate
    idt[index].offset_mid = (handler >> 16) & 0xFFFF;
    idt[index].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[index].zero = 0;
}

void irq_remap()
{
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0x0);
    outb(0xA1, 0x0);
}

void idt_init()
{
    idtr.base = (uint64_t)&idt;
    idtr.limit = sizeof(idt) - 1;

    memset(&idt, 0, sizeof(idt));

    idt_set_entry(0, (uint64_t)&isr0);
    idt_set_entry(1, (uint64_t)&isr1);
    idt_set_entry(2, (uint64_t)&isr2);
    idt_set_entry(3, (uint64_t)&isr3);
    idt_set_entry(4, (uint64_t)&isr4);
    idt_set_entry(5, (uint64_t)&isr5);
    idt_set_entry(6, (uint64_t)&isr6);
    idt_set_entry(7, (uint64_t)&isr7);
    idt_set_entry(8, (uint64_t)&isr8);
    idt_set_entry(9, (uint64_t)&isr9);
    idt_set_entry(10, (uint64_t)&isr10);
    idt_set_entry(11, (uint64_t)&isr11);
    idt_set_entry(12, (uint64_t)&isr12);
    idt_set_entry(13, (uint64_t)&isr13);
    idt_set_entry(14, (uint64_t)&isr14);
    idt_set_entry(15, (uint64_t)&isr15);
    idt_set_entry(16, (uint64_t)&isr16);
    idt_set_entry(17, (uint64_t)&isr17);
    idt_set_entry(18, (uint64_t)&isr18);
    idt_set_entry(19, (uint64_t)&isr19);
    idt_set_entry(20, (uint64_t)&isr20);
    idt_set_entry(21, (uint64_t)&isr21);
    idt_set_entry(22, (uint64_t)&isr22);
    idt_set_entry(23, (uint64_t)&isr23);
    idt_set_entry(24, (uint64_t)&isr24);
    idt_set_entry(25, (uint64_t)&isr25);
    idt_set_entry(26, (uint64_t)&isr26);
    idt_set_entry(27, (uint64_t)&isr27);
    idt_set_entry(28, (uint64_t)&isr28);
    idt_set_entry(29, (uint64_t)&isr29);
    idt_set_entry(30, (uint64_t)&isr30);
    idt_set_entry(31, (uint64_t)&isr31);

    irq_remap();

    idt_set_entry(32, (uint64_t)&irq0);
    idt_set_entry(33, (uint64_t)&irq1);
    idt_set_entry(34, (uint64_t)&irq2);
    idt_set_entry(35, (uint64_t)&irq3);
    idt_set_entry(36, (uint64_t)&irq4);
    idt_set_entry(37, (uint64_t)&irq5);
    idt_set_entry(38, (uint64_t)&irq6);
    idt_set_entry(39, (uint64_t)&irq7);
    idt_set_entry(40, (uint64_t)&irq8);
    idt_set_entry(41, (uint64_t)&irq9);
    idt_set_entry(42, (uint64_t)&irq10);
    idt_set_entry(43, (uint64_t)&irq11);
    idt_set_entry(44, (uint64_t)&irq12);
    idt_set_entry(45, (uint64_t)&irq13);
    idt_set_entry(46, (uint64_t)&irq14);
    idt_set_entry(47, (uint64_t)&irq15);

    memset(&isr_handlers, 0, sizeof(isr_handlers));

    load_idt((uint64_t)&idtr);
}

extern void syscall_handler_asm();

void tables_init()
{
    idt_init();
    gdt_init();
    tss_init();

    // set up the STAR MSR to point to the correct segments
    asm volatile ("wrmsr" ::"a"(0), "d"(0x230008), "c"(0xC0000081));

    // set up the LSTAR MSR to point to the syscall handler
    uint64_t lstar = (uint64_t)&syscall_handler_asm;
    asm volatile ("wrmsr" ::"a"(lstar), "d"(lstar >> 32), "c"(0xC0000082));

    // set up the FMASK MSR to clear the interrupt flag on syscall
    asm volatile ("wrmsr" ::"a"(0x200), "d"(0), "c"(0xC0000084));


    // enable syscall/sysret
    uint64_t efer;
    asm volatile ("rdmsr" : "=a"(efer) : "c"(0xC0000080));
    efer |= 1;
    asm volatile ("wrmsr" ::"a"(efer), "c"(0xC0000080));
}

char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "#GP Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"};

void page_fault_error(regs_t *r)
{
    // later on we will do some code elsewhere to see if this is actually an error (swapping pages, copy on write etc)
    // but for now, we will just print out information about the page fault
    uint64_t faulting_address;
    asm volatile("mov %%cr2, %0" : "=r"(faulting_address));
    uint32_t flags = r->err_code;
    kpanic("Page fault! (%s%s%s%s%s) at 0x%lx [0x%lx]\n", (flags & 0x1) ? "Present |" : "Not present |", (flags & 0x2) ? "Write |" : "Read |", (flags & 0x4) ? "User |" : "Supervisor |", (flags & 0x8) ? "Reserved bit set |" : "", (flags & 0x10) ? "Instruction fetch" : "", (uint64_t)faulting_address, r->rip);
}

void fault_handler(regs_t *regs)
{
    if (regs->int_no == 14)
    {
        page_fault_error(regs);
    } else if (regs->int_no == 6) {
        // memory address A8178 may contain a pointer to a string
        // TODO: validate the pointer, just printing it without verifying DYLD loading is dangerous
        kpanic("Invalid opcode! [0x%lx] Got string:\n%s\n", regs->rip, (char *)*(uint64_t *)0xA8178);
    }
    else
    {
        kpanic("Unhandled exception: %s (error code: %d) [0x%lx]", exception_messages[regs->int_no], regs->err_code, regs->rip);
    }
}

void irq_handler(regs_t *regs)
{
    printf("IRQ: %d\n", regs->int_no - 32);

    if (isr_handlers[regs->int_no - 32] != 0)
    {
        isr_handlers[regs->int_no - 32](regs);
    }

    if (regs->int_no >= 40)
    {
        outb(0xA0, 0x20);
    }

    outb(0x20, 0x20);
}

void register_interrupt_handler(uint8_t n, isr_handler_t handler)
{
    isr_handlers[n] = handler;
}

void jump_to_usermode(uint64_t rip, uint64_t rsp)
{
    asm volatile("cli");

    // set the segment registers for user mode
    // 0x28 | 0x3 = 0x2B for user mode data segment
    // 0x30 | 0x3 = 0x33 for user mode code segment
    asm volatile("mov $0x2B, %ax");
    asm volatile("mov %ax, %ds");
    asm volatile("mov %ax, %es");
    asm volatile("mov %ax, %fs");
    asm volatile("mov %ax, %gs");
    // SS is handled by iret

    // store the pre-syscall rsp
    asm volatile("mov %%rsp, %0" : "=r"(presyscall_rsp));
    asm volatile("mov %%rbp, %0" : "=r"(presyscall_rbp));

    //push data selector
    asm volatile("pushq $0x2B");
    //push rsp
    asm volatile("pushq %0" ::"r"(rsp));
    //push rflags
    asm volatile("pushfq");
    // or the interrupt flag
    asm volatile("orq $0x200, (%%rsp)" ::);

    asm volatile ("xchg %bx, %bx");

    //push code selector
    asm volatile("pushq $0x33");
    //push rip
    asm volatile("pushq %0" ::"r"(rip));
    //iretq
    asm volatile("iretq");
}