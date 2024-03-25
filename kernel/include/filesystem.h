#ifndef _FILESYSTEM_H
#define _FILESYSTEM_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#define PATH_MAX 4096
#define NAME_MAX 255

#define O_RDONLY 0x0
#define O_WRONLY 0x1
#define O_RDWR 0x2
#define O_ACCMODE 0x3
#define O_NONBLOCK 0x4
#define O_APPEND 0x8
#define O_CREAT 0x200
#define O_TRUNC 0x400
#define O_EXCL 0x800
#define FMARK 0x1000
#define FDEFER 0x2000
#define FWASLOCKED 0x4000
#define FHASLOCK FWASLOCKED
#define O_EVTONLY 0x8000
#define FWASWRITTEN 0x10000
#define O_NOCTTY 0x20000
#define FNOCACHE 0x40000
#define FNORDAHEAD 0x80000
#define O_DIRECTORY 0x100000
#define O_SYMLINK 0x200000

#define F_GETPATH 50

typedef struct
{
	char name[NAME_MAX];
	void *inode;
	void *filesystem;
	size_t size;
	uint32_t flags;
} file_t;

typedef struct
{
	char name[NAME_MAX];
	void *inode;
	void *filesystem;
	uint32_t flags;
} dir_t;

typedef struct stat
{
	dev_t st_dev;		 /* inode's device */
	ino_t st_ino;		 /* inode's number */
	mode_t st_mode;		 /* inode protection mode */
	nlink_t st_nlink;	 /* number of hard links */
	uid_t st_uid;		 /* user ID of the file's owner */
	gid_t st_gid;		 /* group ID of the file's group */
	dev_t st_rdev;		 /* device type */
	time_t st_atime;	 /* time of last access */
	long st_atimensec;	 /* nsec of last access */
	time_t st_mtime;	 /* time of last data modification */
	long st_mtimensec;	 /* nsec of last data modification */
	time_t st_ctime;	 /* time of last file status change */
	long st_ctimensec;	 /* nsec of last file status change */
	off_t st_size;		 /* file size, in bytes */
	int64_t st_blocks;	 /* blocks allocated for file */
	uint32_t st_blksize; /* optimal blocksize for I/O */
	uint32_t st_flags;	 /* user defined flags for file */
	uint32_t st_gen;	 /* file generation number */
	int32_t st_lspare;
	int64_t st_qspare[2];
} stat_t;

typedef struct
{
	void *pointer;
	uint64_t value;
} pointer_int_t;

typedef pointer_int_t (*open_func_t)(const char *, uint64_t, void *);
typedef size_t (*read_func_t)(void *, size_t, size_t, void *, void *, uint64_t);
typedef int (*close_func_t)(void *, void *);
typedef int (*fcntl_func_t)(int, long, void *, void *);

typedef size_t (*file_size_func_t)(const char *, void *);

// generic device structure
typedef struct device
{
	char name[NAME_MAX];
	dev_t id;
	uint32_t flags;
	void *data;

	open_func_t open;
	read_func_t read;
	close_func_t close;
	fcntl_func_t fcntl;

	file_size_func_t file_size;

	struct device *next;
} device_t;

typedef struct mount
{
	char filesystemtype[NAME_MAX];
	char path[PATH_MAX];
	unsigned long mountflags;
	device_t *filesystem;
	struct mount *next;
} mount_t;

typedef struct file_descriptor
{
	int descriptor_id;
	uint64_t flags;
	device_t *device;
	void *data;
	struct file_descriptor *next;
} file_descriptor_t;

uint32_t get_path_depth(char *path);
void get_path_part(char *path, char *part, uint32_t part_num);
dev_t get_next_device_id();
void filesystem_init(device_t *ramdisk_device);
int fopen(char *path, int flags);
size_t fread(void *ptr, size_t size, size_t nmemb, int fd);
size_t file_size_internal(char *path);
char *device_to_path(device_t *device);
int fcntl(int fd, int cmd, long arg);
int fclose(int fd);

#endif