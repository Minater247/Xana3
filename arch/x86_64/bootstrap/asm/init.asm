    [bits 32]
    default rel

    section .text

global _start
extern boot_stack_top
extern preboot_load
_start:
    cli
    xchg bx, bx

    ; Set up the initial stack
    mov esp, (boot_stack_top - 0xffffff8000000000)
    mov ebp, esp

    ; Save the multiboot information
    push ebx
    push eax

    mov ecx, preboot_load - 0xffffff8000000000
    call ecx



    hlt