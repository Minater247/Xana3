[bits 64]
default rel

section .text

signal_return:
    mov rax, 0x0F
    mov rdi, 0x00 ; normal signal (no syscall)
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

signal_return_syscall:
    mov rax, 0x0F
    mov rdi, 0x01 ; syscall signal
    syscall
after_signal_return_syscall:

global run_signal_syscall
run_signal_syscall:
    mov rsp, rdi ; argument 1 is user stack pointer
    ; push signal_return contents to user stack so that we have the return code in userspace
    sub rsp, 128 ; skip red zone, if used
    sub rsp, after_signal_return_syscall - signal_return_syscall

    mov rax, rsi ; signal handler pointer
    mov rbx, rcx ; signal pointer

    ; copy signal_return contents to user stack
    mov rdi, rsp ; destination address
    lea rsi, [rel signal_return_syscall] ; source address
    mov rcx, after_signal_return_syscall - signal_return_syscall ; number of bytes to copy
    rep movsb ; copy bytes

    push rsp ; push signal_return address

    ; Rearrange registers for signal handler
    mov rdi, rdx ; signal number
    mov rsi, rbx ; signal info
    mov rdx, r8 ; ucontext

    ; TODO: do this in *user mode*!

    ; Call signal handler
    jmp rax