[bits 64]

section .text
global __init
extern kmain
__init:
    mov rax, kmain
    mov rbx, 0xffffff8000000000
    add rsp, rbx
    add rbp, rbx
    jmp rax