#ifndef _MULTIBOOT_H
#define _MULTIBOOT_H

#include <stdint.h>
#include <stdbool.h>

bool load_multiboot(uint32_t magic, void *mbd);

#endif