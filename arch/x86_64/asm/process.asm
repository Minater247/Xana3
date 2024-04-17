[bits 64]
default rel

section .text

signal_return:
    mov rax, 0x0F ; syscall number for rt_sigreturn
    syscall
after_signal_return:

global run_signal
run_signal:
    mov rsp, rdi ; argument 1 is user stack pointer
    ; push signal_return contents to user stack so that we have the return code in userspace
    sub rsp, 128 ; skip red zone, if used
    sub rsp, after_signal_return - signal_return

    mov rax, rsi ; signal handler pointer
    mov rbx, rcx ; signal pointer

    ; copy signal_return contents to user stack
    mov rdi, rsp ; destination address
    lea rsi, [rel signal_return] ; source address
    mov rcx, after_signal_return - signal_return ; number of bytes to copy
    rep movsb ; copy bytes

    push rsp ; push signal_return address

    ; Rearrange registers for signal handler
    mov rdi, rdx ; signal number
    mov rsi, rbx ; signal info
    mov rdx, r8 ; ucontext

    ; TODO: do this in *user mode*!

    ; Call signal handler
    jmp rax