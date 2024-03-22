// tables.c
// Functions for the initialization of the GDT and IDT.

#include <stdint.h>

#include <tables.h>
#include <hardware.h>
#include <string.h>
#include <errors.h>
#include <io.h>

gdt_entry_t gdt[3];
gdt_ptr_t   gdt_ptr;

idt_entry_t idt[256];
idt_ptr_t   idt_ptr;

extern void gdt_flush(uint32_t);
extern void idt_load(uint32_t);

extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();
extern void isr128();

extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

void *irq_routines[16] = { 0 };

void irq_install_handler(int irq, void (*handler)(regs_t *r)) {
    irq_routines[irq] = handler;
}

void irq_uninstall_handler(int irq) {
    irq_routines[irq] = 0;
}

void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F);

    gdt[num].granularity |= (gran & 0xF0);
    gdt[num].access = access;
}

void gdt_initialize() {
    gdt_ptr.limit = (sizeof(gdt_entry_t) * 5) - 1;
    gdt_ptr.base  = (uint32_t)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    gdt_flush((uint32_t)&gdt_ptr);
}

void idt_set_gate(uint8_t num, uint32_t base, uint16_t selector, uint8_t flags) {
    idt[num].offset_low = (base & 0xFFFF);
    idt[num].offset_high = (base >> 16) & 0xFFFF;

    idt[num].selector = selector;
    idt[num].zero = 0;

    idt[num].type_attr = flags;
}

void isrs_install() {
    idt_set_gate(0, (uint32_t)isr0, 0x8, 0x8E);
    idt_set_gate(1, (uint32_t)isr1, 0x8, 0x8E);
    idt_set_gate(2, (uint32_t)isr2, 0x8, 0x8E);
    idt_set_gate(3, (uint32_t)isr3, 0x8, 0x8E);
    idt_set_gate(4, (uint32_t)isr4, 0x8, 0x8E);
    idt_set_gate(5, (uint32_t)isr5, 0x8, 0x8E);
    idt_set_gate(6, (uint32_t)isr6, 0x8, 0x8E);
    idt_set_gate(7, (uint32_t)isr7, 0x8, 0x8E);
    idt_set_gate(8, (uint32_t)isr8, 0x8, 0x8E);
    idt_set_gate(9, (uint32_t)isr9, 0x8, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x8, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x8, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x8, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x8, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x8, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x8, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x8, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x8, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x8, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x8, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x8, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x8, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x8, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x8, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x8, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x8, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x8, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x8, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x8, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x8, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x8, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x8, 0x8E);

    //syscall IDT entry
    idt_set_gate(128, (uint32_t)isr128, 0x08, 0x8E);
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
    "Reserved"
};

void page_fault_error(regs_t *r) {
    //later on we will do some code elsewhere to see if this is actually an error (swapping pages, copy on write etc)
    //but for now, we will just print out information about the page fault
    uint32_t faulting_address;
    asm volatile("mov %%cr2, %0" : "=r" (faulting_address));
    uint32_t flags = r->err_code;

    // if (current_process->pid == 0) {
    //     kpanic("Page fault! (%s%s%s%s%s) at 0x%x\n", (flags & 0x1) ? "Present |" : "Not present |", (flags & 0x2) ? "Write |" : "Read |", (flags & 0x4) ? "User |" : "Supervisor |", (flags & 0x8) ? "Reserved bit set |" : "", (flags & 0x10) ? "Instruction fetch" : "", faulting_address);
    // } else {
    //     serial_printf("Process %d page fault! (%s%s%s%s%s) at 0x%x\n", current_process->pid, (flags & 0x1) ? "Present |" : "Not present |", (flags & 0x2) ? "Write |" : "Read |", (flags & 0x4) ? "User |" : "Supervisor |", (flags & 0x8) ? "Reserved bit set |" : "", (flags & 0x10) ? "Instruction fetch" : "", faulting_address);
    // }
    kpanic("Page fault! (%s%s%s%s%s) at 0x%x\n", (flags & 0x1) ? "Present |" : "Not present |", (flags & 0x2) ? "Write |" : "Read |", (flags & 0x4) ? "User |" : "Supervisor |", (flags & 0x8) ? "Reserved bit set |" : "", (flags & 0x10) ? "Instruction fetch" : "", faulting_address);
    
    asm volatile (
        "movl $60, %%eax \n\t"
        "movl $1, %%ebx \n\t"
        "int $0x80 \n\t"
        :
        :
        : "%eax", "%ebx"
    );
}

void isr_handler(regs_t *r) {
    if (r->int_no < 32) {
        if (r->int_no == 14) {
            page_fault_error(r);
        }

        kpanic("%s Exception. System Halted!\n", exception_messages[r->int_no]);
    }

    if (r->int_no == 128) {
        //msyscall_handler(r);
    }
}

void irq_remap() {
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

void irq_install() {
    irq_remap();

    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq2, 0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq3, 0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq4, 0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq5, 0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq6, 0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq7, 0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq8, 0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq9, 0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x08, 0x8E);
}

int ticks = 0;

void irq_handler(regs_t *r) {
    void (*handler)(regs_t *r);
    
    if (r->int_no >= 40) {
        outb(0xA0, 0x20);
    }
    
    outb(0x20, 0x20);

    handler = irq_routines[r->int_no - 32];
    if (handler) {
        handler(r);
    }
}

void idt_initialize() {
    idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
    idt_ptr.base = (uint32_t)&idt;

    memset(&idt, 0, sizeof(idt_entry_t) * 256);

    idt_load((uint32_t)&idt_ptr);

    isrs_install();
    irq_install();
}

void tables_initialize() {
    gdt_initialize();
    idt_initialize();
}