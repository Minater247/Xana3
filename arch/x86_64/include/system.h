#ifndef _SYSTEM_H
#define _SYSTEM_H

#include <stdint.h>

#define VIRT_MEM_OFFSET 0xffffff8000000000

#define BOCHS_BREAKPOINT asm volatile("xchgw %bx, %bx");

typedef struct {
   uint64_t r15;
   uint64_t r14;
   uint64_t r13;
   uint64_t r12;
   uint64_t r11;
   uint64_t r10;
   uint64_t r9;
   uint64_t r8;
   uint64_t rdi;
   uint64_t rsi;
   uint64_t rbp;
   uint64_t rbx;
   uint64_t rdx;
   uint64_t rcx;
   uint64_t rax;
   uint64_t int_no;
   uint64_t err_code;
   uint64_t rip; 
   uint64_t cs;
   uint64_t rflags;
   uint64_t rsp;
   uint64_t ss;
} __attribute__((packed)) regs_t;


// Safe inline assembly functions, to be used in C code
// Shouldn't be dangerous, but care be taken when using them
#define ASM_DISABLE_INTERRUPTS asm volatile("cli");
#define ASM_ENABLE_INTERRUPTS asm volatile("sti");

#define ASM_GET_CR2(reg) asm volatile("mov %%cr2, %0" : "=r"(reg));

#define ASM_GET_CR3(reg) asm volatile("mov %%cr3, %0" : "=r"(reg));
#define ASM_SET_CR3(reg) asm volatile("mov %0, %%cr3" ::"r"(reg));

#define ASM_READ_RSP(reg) asm volatile("mov %%rsp, %0" : "=r"(reg));
#define ASM_READ_RBP(reg) asm volatile("mov %%rbp, %0" : "=r"(reg));

// NOT SAFE! Does not mark as clobbered. Only move RBP/RSP if the stack stays the
// same, such as the jump from lower to higher memory at initialization, or if
// the next instruction is a direct jump, call to *assembly*, or return.
#define ASM_WRITE_RBP(reg) asm volatile("mov %0, %%rbp" ::"r"(reg));
#define ASM_WRITE_RSP(reg) asm volatile("mov %0, %%rsp" ::"r"(reg));

#define ASM_OUTB(port, val) asm volatile("outb %0, %1" :: "a"(val), "Nd"(port));
#define ASM_INB(port, val) asm volatile("inb %1, %0" : "=a"(val) : "Nd"(port));

#define ASM_WRMSR_ADC(a, d, c) asm volatile("wrmsr" :: "a"(a), "d"(d), "c"(c));
#define ASM_RDMSR(msr, reg) asm volatile("rdmsr" : "=a"(reg) : "c"(msr));

#define IRQ0 asm volatile ("int $32")

#endif