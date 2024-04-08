default rel

section .text
global load_gdt, load_tss, load_idt, load_gdt_initial

load_gdt_initial:
    lgdt [rdi]  ; Load GDT
    ; Set up segment registers
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

load_gdt:
    lgdt [rdi]  ; Load GDT
    ret

load_idt:
    lidt [rdi]  ; Load IDT
    ret

load_tss:
    ; TSS is the 3rd entry in the GDT (0-indexed)
    mov rax, 0x18
    ltr ax      ; Load TSS
    ret





%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push 0
    push %1
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push %1
    jmp isr_common_stub
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE 8
ISR_NOERRCODE 9
ISR_ERRCODE 10
ISR_ERRCODE 11
ISR_ERRCODE 12
ISR_ERRCODE 13
ISR_ERRCODE 14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_NOERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

%macro pusha64 0
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro popa64 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
%endmacro

extern fault_handler
isr_common_stub:
    pusha64
    mov rdi, rsp ; Give the C function the address of the registers
    mov rsi, 0 ; Clear the register
    mov rsi, cr2 ; Get address of fault, if there was a page fault
    call fault_handler
    popa64
    ; For some reason there's 16 bytes of information pushed outside of pusha64
    add rsp, 0x10
    iretq


%macro IRQ 1
global irq%1
irq%1:
    push 0
    ; We need to push %1 + 32 because the first 32 interrupts are reserved for exceptions
    push %1 + 32
    jmp irq_common_stub
%endmacro

IRQ 0
IRQ 1
IRQ 2
IRQ 3
IRQ 4
IRQ 5
IRQ 6
IRQ 7
IRQ 8
IRQ 9
IRQ 10
IRQ 11
IRQ 12
IRQ 13
IRQ 14
IRQ 15

extern irq_handler

irq_common_stub:
    pusha64
    mov rdi, rsp ; Give the C function the address of the registers
    mov rsi, 0 ; We don't need to pass anything in cr2
    call irq_handler
    popa64
    ; For some reason there's 16 bytes of information pushed outside of pusha64
    add rsp, 0x10
    iretq


; typedef struct {
;    uint64_t r15;
;    uint64_t r14;
;    uint64_t r13;
;    uint64_t r12;
;    uint64_t r11;
;    uint64_t r10;
;    uint64_t r9;
;    uint64_t r8;
;    uint64_t rdi;
;    uint64_t rsi;
;    uint64_t rbp;
;    uint64_t rbx;
;    uint64_t rdx;
;    uint64_t rcx;
;    uint64_t rax;
;    uint64_t int_no;
;    uint64_t err_code;
;    uint64_t rip; 
;    uint64_t cs;
;    uint64_t rflags;
;    uint64_t rsp;
;    uint64_t ss;
; } __attribute__((packed)) regs_t;



global syscall_handler_asm
extern syscall_handler
syscall_handler_asm:
    cli ; Disable interrupts

    ; Save the old RSP
    mov qword [syscall_old_rsp], rsp
    mov rsp, syscall_stack_top

    ; Should be fine to do actual syscall now!
    ; We need to follow the C struct layout for the registers so we can use
    ; the same order in the C code

    ; We have to push dummy values for the registers that are not used, SS and CS, notably
    push 0 ; SS
    push qword [syscall_old_rsp]
    pushfq
    push 0 ; CS
    push 0 ; RIP
    push 0 ; Error code
    push 0 ; int_no
    pusha64

    ; Give one argument, the address of the registers
    mov rdi, rsp
    call syscall_handler

after_syscall:
    mov rsp, rax ; return value is new regs pointer

    ; Pop the registers (rax in this struct has been set to the return value)
    popa64

    mov rsp, qword [syscall_old_rsp]

    ; Return from the syscall
    o64 sysret

section .data
global syscall_stack
global syscall_stack_top
; Small temporary stack for syscalls to get to C code
syscall_stack:
    times 4096 db 0
syscall_stack_top:

global syscall_old_rsp
syscall_old_rsp: dq 0

global read_rip
read_rip:
    ; Read the RIP of the calling function
    mov rax, [rsp]
    ret


global jump_to_usermode
jump_to_usermode:
    cli

    ; set the segment registers for user mode
    ; 0x28 | 0x3 = 0x2B for user mode data segment
    ; 0x30 | 0x3 = 0x33 for user mode code segment
    mov ax, 0x2B
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    ; SS is handled by iret

    ; DS
    push 0x2B
    ; RSP
    push rsi
    ; Also set RBP to the same value
    mov rbp, rsi
    ; RFLAGS
    pushfq
    ; OR the interrupt flag
    or qword [rsp], 0x200
    ; CS
    push 0x33
    ; RIP
    push rdi
    ; IRETQ
    iretq