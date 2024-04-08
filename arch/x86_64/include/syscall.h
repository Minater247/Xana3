#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <stdint.h>
#include <system.h>

typedef int64_t (*syscall_t)(regs_t *regs);

#define SYSCALL_STACK_SIZE 4096

void syscall_init();
uint64_t syscall_handler(regs_t *regs);

#endif