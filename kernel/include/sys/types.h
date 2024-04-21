#ifndef _TYPES_H
#define _TYPES_H

#include <stdint.h>


typedef	uint64_t		u_quad_t;
typedef	int64_t			quad_t;
typedef	quad_t *		qaddr_t;

typedef long        time_t;	    // time
 
 // TODO: figure out how to swap types in newlib, notably the gid_t and ino_t types
typedef	char *		caddr_t;    // core address
typedef	uint64_t		daddr_t;    // disk address
typedef	uint16_t		dev_t;	    // device number
typedef	uint16_t	gid_t;	    // group id
typedef	uint16_t	ino_t;	    // inode number
typedef	int32_t	mode_t;	    // permissions
typedef	uint16_t	nlink_t;    // link count
typedef	quad_t		off_t;	    // file offset
typedef	int32_t		pid_t;	    // process id
typedef quad_t		rlim_t;	    // resource limit
typedef	int32_t		segsz_t;    // segment size
typedef	int32_t		swblk_t;    // swap offset
typedef	uint16_t	uid_t;	    // user id

typedef uint64_t blksize_t;
typedef uint64_t blkcnt_t;

typedef	uint32_t	socklen_t;
typedef uint16_t sa_family_t;


#endif