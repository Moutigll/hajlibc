/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file stat.h
 * @brief struct stat layout and file mode constants.
 * @Created: 2026/11/22 00:20:57 by Moutig
 * @Updated: 2026/11/22 00:30:56 by Moutig
 *
 * Defines struct stat for the Linux kernel ABI of x86_64
 * (arch/x86) and aarch64 (asm-generic/stat.h). The layout must not
 * change: it is filled directly by the kernel through fstat/newfstatat.
 *
 * Also defines the S_IFMT / S_IF* file-type bits and the S_IS*
 * classification macros, which stdio needs to tell a regular file
 * from a pipe, a tty or a socket.
 */

#ifndef _BITS_STAT_H
# define _BITS_STAT_H

# include <bits/arch.h>
# include <bits/types.h>
# include <bits/time.h>

/* ----- File type bits (st_mode & S_IFMT) ----- */

/** Bit mask for the file-type part of st_mode. */
# define S_IFMT		0170000

/** FIFO (named pipe). */
# define S_IFIFO	0010000

/** Character device. */
# define S_IFCHR	0020000

/** Directory. */
# define S_IFDIR	0040000

/** Block device. */
# define S_IFBLK	0060000

/** Regular file. */
# define S_IFREG	0100000

/** Symbolic link. */
# define S_IFLNK	0120000

/** Socket. */
# define S_IFSOCK	0140000


/* ----- File type test macros ----- */

/** True if @p m is a FIFO. */
# define S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO)

/** True if @p m is a character device. */
# define S_ISCHR(m)		(((m) & S_IFMT) == S_IFCHR)

/** True if @p m is a directory. */
# define S_ISDIR(m)		(((m) & S_IFMT) == S_IFDIR)

/** True if @p m is a block device. */
# define S_ISBLK(m)		(((m) & S_IFMT) == S_IFBLK)

/** True if @p m is a regular file. */
# define S_ISREG(m)		(((m) & S_IFMT) == S_IFREG)

/** True if @p m is a symbolic link. */
# define S_ISLNK(m)		(((m) & S_IFMT) == S_IFLNK)

/** True if @p m is a socket. */
# define S_ISSOCK(m)	(((m) & S_IFMT) == S_IFSOCK)

/* ----- Permission bits (subset, completed with chmod) ----- */

/** Set-user-ID on execution. */
# define S_ISUID 04000

/** Set-group-ID on execution. */
# define S_ISGID 02000

/** Sticky bit. */
# define S_ISVTX 01000

/** Read by owner. */
# define S_IRUSR 00400

/** Write by owner. */
# define S_IWUSR 00200

/** Execute/search by owner. */
# define S_IXUSR 00100

/** Read by group. */
# define S_IRGRP 00040

/** Write by group. */
# define S_IWGRP 00020

/** Execute/search by group. */
# define S_IXGRP 00010

/** Read by others. */
# define S_IROTH 00004

/** Write by others. */
# define S_IWOTH 00002

/** Execute/search by others. */
# define S_IXOTH 00001


/* ----- struct stat (asm-generic ABI) ----- */

/**
 * @brief File status.
 *
 * Field order and types mirror the kernel's struct stat, which differs
 * between x86_64 (arch/x86, 144 bytes) and the asm-generic ABI used by
 * aarch64 (128 bytes: 32-bit st_mode / st_nlink / st_blksize, extra
 * padding). Do not reorder, do not remove the padding.
 *
 * The timestamps are exposed as struct timespec (POSIX.1-2008+),
 * with the historical st_atime / st_mtime / st_ctime macros kept
 * for code that predates them.
 */
# if defined(HAJ_ARCH_X86_64)
struct stat
{
	dev_t		st_dev;			/* Device ID of the containing file. */
	ino_t		st_ino;			/* Inode number. */
	nlink_t		st_nlink;		/* Number of hard links. */
	mode_t		st_mode;		/* File type and permissions. */
	uid_t		st_uid;			/* Owner user ID. */
	gid_t		st_gid;			/* Owner group ID. */
	int			__pad0;			/* Padding, kernel ABI. */
	dev_t		st_rdev;		/* Device ID (special files only). */
	off_t		st_size;		/* File size in bytes. */
	blksize_t	st_blksize;		/* Preferred I/O block size. */
	blkcnt_t	st_blocks;		/* Number of 512-byte blocks. */
	struct timespec	st_atim;	/* Last access time. */
	struct timespec	st_mtim;	/* Last modification time. */
	struct timespec	st_ctim;	/* Last status change time. */
	long		__unused[3];	/* Padding, kernel ABI. */
};
# else
struct stat
{
	dev_t		st_dev;			/* Device ID of the containing file. */
	ino_t		st_ino;			/* Inode number. */
	mode_t		st_mode;		/* File type and permissions. */
	nlink_t		st_nlink;		/* Number of hard links. */
	uid_t		st_uid;			/* Owner user ID. */
	gid_t		st_gid;			/* Owner group ID. */
	dev_t		st_rdev;		/* Device ID (special files only). */
	unsigned long	__pad1;		/* Padding, kernel ABI. */
	off_t		st_size;		/* File size in bytes. */
	blksize_t	st_blksize;		/* Preferred I/O block size. */
	int			__pad2;			/* Padding, kernel ABI. */
	blkcnt_t	st_blocks;		/* Number of 512-byte blocks. */
	struct timespec	st_atim;	/* Last access time. */
	struct timespec	st_mtim;	/* Last modification time. */
	struct timespec	st_ctim;	/* Last status change time. */
	unsigned int	__unused[2];	/* Padding, kernel ABI. */
};
# endif

/* ----- POSIX.1-2001 compatibility macros ----- */

/** Last access time, in seconds. */
# define st_atime	st_atim.tv_sec

/** Last modification time, in seconds. */
# define st_mtime	st_mtim.tv_sec

/** Last status change time, in seconds. */
# define st_ctime	st_ctim.tv_sec

#endif /* _BITS_STAT_H */
