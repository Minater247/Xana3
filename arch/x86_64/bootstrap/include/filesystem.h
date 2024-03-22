#ifndef _FILESYSTEM_H
#define _FILESYSTEM_H

#include <stdint.h>

uint32_t get_path_depth(char *path);
void get_path_part(char *path, char *part, uint32_t part_num);

#endif