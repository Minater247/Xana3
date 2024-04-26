#ifndef _FILESYSTEM_H
#define _FILESYSTEM_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <device.h>

#define PATH_MAX 4096
#define NAME_MAX 255

#define O_ACCMODE (03 | O_PATH)
#define O_RDONLY   00
#define O_WRONLY   01
#define O_RDWR     02

#define O_CREAT         0100
#define O_EXCL          0200
#define O_NOCTTY        0400
#define O_TRUNC        01000
#define O_APPEND       02000
#define O_NONBLOCK     04000
#define O_DSYNC       010000
#define O_ASYNC       020000
#define O_DIRECT      040000
#define O_DIRECTORY  0200000
#define O_NOFOLLOW   0400000
#define O_CLOEXEC   02000000
#define O_SYNC      04010000
#define O_RSYNC     04010000
#define O_LARGEFILE  0100000
#define O_NOATIME   01000000
#define O_TMPFILE  020000000

#define O_EXEC O_PATH
#define O_SEARCH O_PATH

#define F_DUPFD  0
#define F_GETFD  1
#define F_SETFD  2
#define F_GETFL  3
#define F_SETFL  4

#define F_SETOWN 8
#define F_GETOWN 9
#define F_SETSIG 10
#define F_GETSIG 11

#define F_GETLK 5
#define F_SETLK 6
#define F_SETLKW 7

#define F_SETOWN_EX 15
#define F_GETOWN_EX 16

#define F_GETOWNER_UIDS 17

#define F_DUPFD_CLOEXEC 1030
#define F_ADD_SEALS 1033
#define F_GET_SEALS 1034

#define F_SEAL_SEAL 0x0001
#define F_SEAL_SHRINK 0x0002
#define F_SEAL_GROW 0x0004
#define F_SEAL_WRITE 0x0008

#define F_RDLCK 0
#define F_WRLCK 1
#define F_UNLCK 2

#define FD_CLOEXEC 1

#define AT_FDCWD -100
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR 0x200
#define AT_SYMLINK_FOLLOW 0x400
#define AT_EACCESS 0x200
#define AT_EMPTY_PATH 0x1000

#define AT_STATX_FORCE_SYNC 0x2000
#define AT_STATX_DONT_SYNC 0x4000
#define AT_STATX_SYNC_TYPE 0x6000

#define F_GETPATH 50

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define S_IFMT  00170000
#define S_IFSOCK 0140000
#define S_IFLNK	 0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

#define S_ISLNK(m)	(((m) & S_IFMT) == S_IFLNK)
#define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)	(((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m)	(((m) & S_IFMT) == S_IFSOCK)

#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100

#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010

#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001

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

struct dirent64
{
	ino_t d_ino;		   /* inode number */
	off_t d_off;		   /* offset to the next dirent */
	unsigned short d_reclen; /* length of this record */
	char d_name[];		   /* filename */
};

struct stat
{
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
};

#define	FD_SETSIZE	64

typedef	unsigned long	fd_mask;
#define	NFDBITS	(sizeof (fd_mask) * 8)	/* bits per mask */
#define	_howmany(x,y)	(((x)+((y)-1))/(y))

/* We use a macro for fd_set so that including Sockets.h afterwards
   can work.  */
typedef	struct _types_fd_set {
	fd_mask	fds_bits[_howmany(FD_SETSIZE, NFDBITS)];
} fd_set;

struct timeval {
	time_t		tv_sec;		/* seconds */
	suseconds_t	tv_usec;	/* and microseconds */
};




uint32_t get_path_depth(char *path);
void get_path_part(char *path, char *part, uint32_t part_num);
void filesystem_init(device_t *ramdisk_device);
int kfopen(char *path, int flags, mode_t mode);
size_t kfread(void *ptr, size_t size, size_t nmemb, int fd);
size_t file_size_internal(char *path);
char *device_to_path(device_t *device);
int kfcntl(int fd, int cmd, long arg);
int kfclose(int fd);
int mount_at(char *path, device_t *device, char *filesystemtype, unsigned long mountflags);
size_t kfwrite(void *ptr, size_t size, size_t nmemb, int fd);
size_t kfgetdents64(int fd, void *ptr, size_t count);
int kfsetpwd(char *new_pwd);
char *kfgetpwd(char *buf, size_t size);
off_t kflseek(int fd, off_t offset, int whence);
int kioctl(int fd, unsigned long request, void *arg);
int kfstat(int fd, struct stat *buf);
int kstat(char *path, struct stat *buf);
file_descriptor_t *clone_file_descriptors(file_descriptor_t *descriptors);
int kselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *timeout);

#endif