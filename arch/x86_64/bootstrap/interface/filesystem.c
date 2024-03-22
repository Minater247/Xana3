#include <stdint.h>

#include <string.h>

uint32_t get_path_depth(char *path) {
    uint32_t depth = 0;
    for (uint32_t i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/' && path[i + 1] != '\0' && path[i + 1] != '/') {
            depth++;
        }
    }
    return depth;
}

// Get the part of a path (absolute only!) at a certain depth
// Caller must ensure that part_num is less than the depth of the path,
// the buffer is large enough, and that the path is absolute. Behavior is
// undefined if these conditions are not met.
void get_path_part(char *path, char *part, uint32_t part_num) {
    uint32_t current_part = 0;
    uint32_t j = 0;
    for (uint32_t i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            while (path[i + 1] == '/') {
                i++;
            }
            if (current_part == part_num) {
                i += 1;
                for (j = 0; path[i + j] != '/' && path[i + j] != '\0'; j++) {
                    part[j] = path[i + j];
                }
                part[j] = '\0';
                return;
            }
            current_part++;
        } else {
            if (current_part == part_num) {
                part[i] = path[i];
            }
        }
    }   
}