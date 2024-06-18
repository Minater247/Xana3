[bits 64]

section .text
global __init
extern kmain
__init:
    mov rax, kmain
    mov rbx, 0xffffff8000000000
    add rsp, rbx
    add rbp, rbx
    ; align stack to 16 bytes, per ABI
    and rsp, 0xfffffffffffffff0
    jmp rax