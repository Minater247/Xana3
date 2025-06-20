#ifndef _TRACE_H
#define _TRACE_H

#include <stddef.h>
#include <stdint.h>
#include <info.h>

void traceback(size_t depth);
void serial_traceback(size_t depth, uint64_t *rbp);
void traceback_init(kernel_info_t *info);

#endif