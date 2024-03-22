    [bits 32]
    default rel

    section .text

global _start
extern boot_stack_top
extern preboot_load
_start:
    cli

    ; Set up the initial stack
    mov esp, boot_stack_top
    mov ebp, esp

    ; Save the multiboot information
    push ebx
    push eax

    call preboot_load

    hlt