#ifndef _RAMDISK_H
#define _RAMDISK_H

#include <stdint.h>
#include <filesystem.h>

#define FILE_ENTRY 0xBAE7
#define DIR_ENTRY 0x7EAB

typedef struct
{
    uint16_t magic;
    uint32_t file_data;
    char file_name[64];
    uint32_t size;
    // pad to 80 bytes
    uint32_t pad1;
    uint16_t pad2;
} __attribute__((packed)) ramdisk_file_t;

typedef struct {
    uint32_t size;
    uint32_t files;
    uint32_t hdr_size;
    uint32_t root_ents;
} __attribute__((packed)) ramdisk_hdr_t;

typedef struct {
    ramdisk_hdr_t *hdr;
    ramdisk_file_t *files;
    void *data;
} __attribute__((packed)) ramdisk_t;

typedef struct {
    uint32_t hdr;
    uint32_t files;
    uint32_t data;
} __attribute__((packed)) ramdisk_info_32_t;

typedef struct {
    ramdisk_file_t *file;
    uint64_t read_pos;
    uint64_t dependents;
} ramdisk_file_entry_t;

ramdisk_file_t *ramdisk_get_path_header(char *path, ramdisk_t *ramdisk);
char *ramdisk_get_path_data(char *path, ramdisk_t *ramdisk);
char *ramdisk_get_header_data(ramdisk_file_t *header, ramdisk_t *ramdisk);
device_t *init_ramdisk_device(uint64_t addr);
size_t read_dirents(void *ptr, size_t count, void *file_entry_passed, void *device_passed);


extern ramdisk_t boot_ramdisk;

#endif