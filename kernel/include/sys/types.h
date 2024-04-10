#ifndef _TYPES_H
#define _TYPES_H

#include <stdint.h>


typedef	uint64_t		u_quad_t;
typedef	int64_t			quad_t;
typedef	quad_t *		qaddr_t;

typedef long        time_t;	    // time

typedef	char *		caddr_t;    // core address
typedef	int32_t		daddr_t;    // disk address
typedef	int32_t		dev_t;	    // device number
typedef	uint32_t	fixpt_t;    // fixed point number
typedef	uint32_t	gid_t;	    // group id
typedef	uint32_t	ino_t;	    // inode number
typedef	long		key_t;	    // IPC key (for Sys V IPC)
typedef	uint16_t	mode_t;	    // permissions
typedef	uint16_t	nlink_t;    // link count
typedef	quad_t		off_t;	    // file offset
typedef	int32_t		pid_t;	    // process id
typedef quad_t		rlim_t;	    // resource limit
typedef	int32_t		segsz_t;    // segment size
typedef	int32_t		swblk_t;    // swap offset
typedef	uint32_t	uid_t;	    // user id

typedef uint32_t blksize_t;
typedef uint32_t blkcnt_t;


typedef int64_t fpos_t;


#endif