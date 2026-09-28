/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file mman.h
 * @brief Internal definitions of memory mapping constants for each supported OS.
 * @Created: 2026/09/26 04:34:50 by Moutig
 * @Updated: 2026/09/26 06:40:18 by Moutig
 *
 * Internal to hajlib. Defines the values of mmap-related
 * constants for each supported OS. The public <sys/mman.h>
 * includes this header.
 */

#ifndef _BITS_MMAN_H
# define _BITS_MMAN_H

# include <bits/os.h>

/* ----- Linux ----- */

# if defined(HAJ_OS_LINUX)

/* ----- POSIX shared memory ----- */
#define SHM_PREFIX		"/dev/shm"
#define SHM_PREFIX_LEN	(sizeof(SHM_PREFIX) - 1)	/* skip NUL */
#define SHM_PATH_MAX	256


/* Protection flags (same on x86_64 and aarch64). */
#  define PROT_NONE			0x0
#  define PROT_READ			0x1
#  define PROT_WRITE		0x2
#  define PROT_EXEC			0x4
#  define PROT_GROWSDOWN	0x01000000
#  define PROT_GROWSUP		0x02000000

/* Mapping flags. */
#  define MAP_SHARED			0x01
#  define MAP_PRIVATE			0x02
#  define MAP_SHARED_VALIDATE	0x03
#  define MAP_TYPE				0x0f
#  define MAP_FIXED				0x10
#  define MAP_ANONYMOUS			0x20
#  define MAP_ANON				MAP_ANONYMOUS
#  define MAP_32BIT				0x40
#  define MAP_GROWSDOWN			0x00100
#  define MAP_DENYWRITE			0x00800
#  define MAP_EXECUTABLE		0x01000
#  define MAP_LOCKED			0x02000
#  define MAP_NORESERVE			0x04000
#  define MAP_POPULATE			0x08000
#  define MAP_NONBLOCK			0x10000
#  define MAP_STACK				0x20000
#  define MAP_HUGETLB			0x40000
#  define MAP_SYNC				0x80000
#  define MAP_FIXED_NOREPLACE	0x100000

/* msync flags. */
#  define MS_ASYNC		1
#  define MS_INVALIDATE	2
#  define MS_SYNC		4

/* madvise advice values. */
#  define MADV_NORMAL		0
#  define MADV_RANDOM		1
#  define MADV_SEQUENTIAL	2
#  define MADV_WILLNEED		3
#  define MADV_DONTNEED		4
#  define MADV_FREE			8
#  define MADV_REMOVE		9
#  define MADV_DONTFORK		10
#  define MADV_DOFORK		11
#  define MADV_MERGEABLE	12
#  define MADV_UNMERGEABLE	13
#  define MADV_HUGEPAGE		14
#  define MADV_NOHUGEPAGE	15
#  define MADV_DONTDUMP		16
#  define MADV_DODUMP		17
#  define MADV_WIPEONFORK	18
#  define MADV_KEEPONFORK	19
#  define MADV_COLD			20
#  define MADV_PAGEOUT		21
#  define MADV_HWPOISON		100

/* mlockall flags. */
#  define MCL_CURRENT	1
#  define MCL_FUTURE	2
#  define MCL_ONFAULT	4

# endif /* HAJ_OS_LINUX */

/* ----- FreeBSD ----- */

# if defined(HAJ_OS_FREEBSD)

#  define PROT_NONE		0x0
#  define PROT_READ		0x1
#  define PROT_WRITE	0x2
#  define PROT_EXEC		0x4

#  define MAP_SHARED		0x0001
#  define MAP_PRIVATE		0x0002
#  define MAP_FIXED			0x0010
#  define MAP_RENAME		0x0020
#  define MAP_NORESERVE		0x0040
#  define MAP_HASSEMAPHORE	0x0200
#  define MAP_STACK			0x0400
#  define MAP_NOSYNC		0x0800
#  define MAP_ANON			0x1000
#  define MAP_ANONYMOUS		MAP_ANON
#  define MAP_EXCL			0x4000
#  define MAP_PREFAULT_READ	0x40000
#  define MAP_GUARD			0x00008000

#  define MS_ASYNC		1
#  define MS_INVALIDATE	2
#  define MS_SYNC		4

#  define MADV_NORMAL		0
#  define MADV_RANDOM		1
#  define MADV_SEQUENTIAL	2
#  define MADV_WILLNEED		3
#  define MADV_DONTNEED		4
#  define MADV_FREE			5
#  define MADV_NOSYNC		6
#  define MADV_AUTOSYNC		7
#  define MADV_NOCORE		8
#  define MADV_CORE			9
#  define MADV_PROTECT		10

#  define MCL_CURRENT	1
#  define MCL_FUTURE	2

# endif /* HAJ_OS_FREEBSD */

/* ----- Darwin (macOS, iOS) ----- */

# if defined(HAJ_OS_DARWIN)

/* ----- POSIX shared memory ----- */
#define SHM_PREFIX		"/var/tmp/.hajlib-shm-"
#define SHM_PREFIX_LEN	(sizeof(SHM_PREFIX) - 1)
#define SHM_PATH_MAX	256

#  define PROT_NONE		0x0
#  define PROT_READ		0x1
#  define PROT_WRITE	0x2
#  define PROT_EXEC		0x4

#  define MAP_SHARED					0x0001
#  define MAP_PRIVATE					0x0002
#  define MAP_FIXED						0x0010
#  define MAP_RENAME					0x0020
#  define MAP_NORESERVE					0x0040
#  define MAP_ANON						0x1000
#  define MAP_ANONYMOUS					MAP_ANON
#  define MAP_JIT						0x0800
#  define MAP_NOCACHE					0x0400
#  define MAP_HASSEMAPHORE				0x0200
#  define MAP_STACK						0x4000
#  define MAP_RESILIENT_CODESIGN		0x2000
#  define MAP_RESILIENT_MEDIA			0x4000
#  define MAP_TRANSLATED_ALLOW_EXECUTE	0x20000

#  define MS_ASYNC		1
#  define MS_INVALIDATE	2
#  define MS_SYNC		0x0010

#  define MADV_NORMAL		0
#  define MADV_RANDOM		1
#  define MADV_SEQUENTIAL	2
#  define MADV_WILLNEED		3
#  define MADV_DONTNEED		4
#  define MADV_FREE			5
#  define MADV_ZERO			11

#  define MCL_CURRENT	1
#  define MCL_FUTURE	2

# endif /* HAJ_OS_DARWIN */

#endif /* _BITS_MMAN_H */
