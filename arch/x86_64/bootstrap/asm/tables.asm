section .text
global gdt_flush
global idt_load

; Takes one argument from C: the address of the GDT
gdt_flush:
    mov eax, [esp+4]
    lgdt [eax]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:gdt_flush_2 ; jump to kernel code segment
gdt_flush_2:
    ret

; Takes one argument from C: the address of the IDT
idt_load:
    mov eax, [esp+4]
    lidt [eax]
    ret

; ISR definitions
%macro ISR_NOERCODE 1
    align 4
    global isr%1
    isr%1:
        cli
        push 0
        push %1
        jmp isr_common_stub
%endmacro

%macro ISR_WITHCODE 1
    align 4
    global isr%1
    isr%1:
        cli
        push %1
        jmp isr_common_stub
%endmacro

; ISR stubs
ISR_NOERCODE 0
ISR_NOERCODE 1
ISR_NOERCODE 2
ISR_NOERCODE 3
ISR_NOERCODE 4
ISR_NOERCODE 5
ISR_NOERCODE 6
ISR_NOERCODE 7
ISR_WITHCODE 8
ISR_NOERCODE 9
ISR_WITHCODE 10
ISR_WITHCODE 11
ISR_WITHCODE 12
ISR_WITHCODE 13
ISR_WITHCODE 14
ISR_NOERCODE 15
ISR_NOERCODE 16
ISR_NOERCODE 17
ISR_NOERCODE 18
ISR_NOERCODE 19
ISR_NOERCODE 20
ISR_NOERCODE 21
ISR_NOERCODE 22
ISR_NOERCODE 23
ISR_NOERCODE 24
ISR_NOERCODE 25
ISR_NOERCODE 26
ISR_NOERCODE 27
ISR_NOERCODE 28
ISR_NOERCODE 29
ISR_NOERCODE 30
ISR_NOERCODE 31

ISR_NOERCODE 128

; ISR common stub
extern isr_handler
isr_common_stub:
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Push the stack pointer
    mov eax, esp
    push eax

    call isr_handler

    ; Pop the stack pointer
    pop eax
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iret

; IRQ definitions
%macro IRQ 1
    align 4
    global irq%1
    irq%1:
        cli
        push 0
        push %1+32
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

; IRQ common stub
extern irq_handler
irq_common_stub:
    pusha
    push ds
    push es
    push fs
    push gs
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Push the stack pointer
    mov eax, esp
    push eax

    call irq_handler

    ; Pop the stack pointer
    pop eax
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8
    iret

global gdt_flush64
extern passed_info
gdt_flush64:
    mov eax, [esp+4]
    mov ebx, [esp+8]
    lgdt [eax]
    mov dword [esp + 4], 0
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:gdt_flush_2_64 ; jump to kernel code segment
gdt_flush_2_64:
    mov edi, passed_info
    jmp ebx