#ifndef _SYSCALL_H
#define _SYSCALL_H

#include <stdint.h>
#include <system.h>

typedef int64_t (*syscall_t)(regs_t *regs);

void syscall_init();
int64_t syscall_handler(regs_t *regs);

#endif