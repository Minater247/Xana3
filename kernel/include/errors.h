#ifndef _ERRORS_H
#define _ERRORS_H

#include <stdbool.h>
#include <stdarg.h>

#include <display.h>
#include <serial.h>
#include <video.h>
#include <trace.h>
#include <process.h>
#include <memory.h>

// Essentially a nonreturning printf - prints the message and halts the system
#define kpanic(msg, ...) do { \
    serial_printf("Kernel panic: "); \
    serial_printf("In function: %s\n", __func__); \
    serial_printf("%s:%d: \n", __FILE__, __LINE__); \
    serial_printf(msg, ##__VA_ARGS__); \
    serial_printf(" System halted.\n"); \
    serial_traceback(10, NULL); \
    enableBackground(true); \
    kprintf("\033[97;41mKernel panic [PID %d]: ", current_process->pid); \
    kprintf("In function: %s\n", __func__); \
    kprintf("%s:%d: \n", __FILE__, __LINE__); \
    kprintf(msg, ##__VA_ARGS__); \
    kprintf(" System halted.\n"); \
    traceback(10); \
    asm volatile("hlt"); \
    while (1); \
} while (0)
// We should never get to the while (1), but it appeases the compiler saying that
// control reaches the end of a non-void function

// Simple assertion - prints the expression that failed and halts the system
#define kassert(expression) do { \
    if (!(expression)) { \
        kpanic("Assertion failed: %s\n", #expression); \
    } \
} while (0)

#define kassert_msg(expression, msg, ...) do { \
    if (!(expression)) { \
        kpanic(msg, ##__VA_ARGS__); \
    } \
} while (0)

// Prints a special unimplemented error message and halts the system
#define unimplemented(msg) do { \
    kpanic("Unimplemented error: %s", msg); \
} while (0)

// Prints a warning in yellow, but doesn't halt the system
#define kwarn(msg, ...) do { \
    enableBackground(true); \
    kprintf("[\033[93;40mWARN\033[0m]: "); \
    kprintf(msg, ##__VA_ARGS__); \
    kprintf("\033[0m\n"); \
} while (0)


#endif