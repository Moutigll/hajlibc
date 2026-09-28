/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file fcntl.h
 * @brief File control flags per OS.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 16:01:21 by Moutig
 *
 * This header defines the O_* flags (used by open) and the F_*
 * commands (used by fcntl) with the values used by the target OS.
 * The values are ABI-specific: Linux, FreeBSD, and Darwin all use
 * different numbering.
 *
 * Do NOT include this header directly from user code. Use
 * <fcntl.h> instead.
 */

#ifndef _BITS_FCNTL_H
# define _BITS_FCNTL_H

# include <bits/os.h>

# ifndef AT_FDCWD
#  if defined(HAJ_OS_LINUX) || defined(HAJ_OS_FREEBSD)
#   define AT_FDCWD			(-100)
#  elif defined(HAJ_OS_DARWIN)
#   define AT_FDCWD			(-2)
#  else
#   define AT_FDCWD			(-100)
#  endif
# endif

/* ----- Linux ----- */

# if defined(HAJ_OS_LINUX)

/*
 * open() flags. Values are in octal, matching the Linux kernel ABI.
 * The low 2 bits are the access mode, and must be combined with
 * O_RDONLY, O_WRONLY, or O_RDWR.
 */

#  define O_RDONLY		00000000	/* Open for reading only */
#  define O_WRONLY		00000001	/* Open for writing only */
#  define O_RDWR		00000002	/* Open for reading and writing */
#  define O_ACCMODE		00000003	/* Mask for access mode */

#  define O_CREAT		00000100	/* Create file if it does not exist */
#  define O_EXCL		00000200	/* Fail if O_CREAT and file exists */
#  define O_NOCTTY		00000400	/* Do not assign controlling terminal */
#  define O_TRUNC		00001000	/* Truncate file to zero length */
#  define O_APPEND		00002000	/* Append mode */
#  define O_NONBLOCK	00004000	/* Non-blocking mode */
#  define O_NDELAY		O_NONBLOCK	/* Historical synonym */

#  define O_DSYNC		00010000	/* Synchronous data writes */
#  define O_ASYNC		00020000	/* Signal-driven I/O */
#  define O_DIRECT		00040000	/* Direct I/O (no caching) */
#  define O_LARGEFILE	00100000	/* Large file support (32-bit) */
#  define O_DIRECTORY	00200000	/* Must be a directory */
#  define O_NOFOLLOW	00400000	/* Do not follow symlinks */
#  define O_NOATIME		01000000	/* Do not update access time */
#  define O_CLOEXEC		02000000	/* Close on exec */
#  define O_SYNC		04010000	/* Synchronous writes (data + metadata) */
#  define O_PATH		010000000	/* Path-only file descriptor */
#  define O_TMPFILE		020000000	/* Anonymous temporary file */

/*
 * fcntl() commands.
 */

#  define F_DUPFD		0	/* Duplicate file descriptor */
#  define F_GETFD		1	/* Get file descriptor flags */
#  define F_SETFD		2	/* Set file descriptor flags */
#  define F_GETFL		3	/* Get file status flags */
#  define F_SETFL		4	/* Set file status flags */
#  define F_GETLK		5	/* Get record locking information */
#  define F_SETLK		6	/* Set record locking information */
#  define F_SETLKW		7	/* Set record locking and wait */
#  define F_SETOWN		8	/* Set owner for SIGIO/SIGURG */
#  define F_GETOWN		9	/* Get owner for SIGIO/SIGURG */
#  define F_SETSIG		10	/* Set signal for SIGIO */
#  define F_GETSIG		11	/* Get signal for SIGIO */

#  define F_DUPFD_CLOEXEC 1030	/* Duplicate with close-on-exec */

/*
 * File descriptor flags for F_GETFD / F_SETFD.
 */

#  define FD_CLOEXEC	1	/* Close on exec */

/*
 * Lock types for struct flock.
 */

#  define F_RDLCK		0	/* Read lock */
#  define F_WRLCK		1	/* Write lock */
#  define F_UNLCK		2	/* Remove lock */

/* ----- FreeBSD ----- */

# elif defined(HAJ_OS_FREEBSD)

/*
 * open() flags. Values are in hexadecimal.
 */

#  define O_RDONLY		0x0000	/* Open for reading only */
#  define O_WRONLY		0x0001	/* Open for writing only */
#  define O_RDWR		0x0002	/* Open for reading and writing */
#  define O_ACCMODE		0x0003	/* Mask for access mode */

#  define O_NONBLOCK	0x0004	/* Non-blocking mode */
#  define O_APPEND		0x0008	/* Append mode */
#  define O_SHLOCK		0x0010	/* Shared lock */
#  define O_EXLOCK		0x0020	/* Exclusive lock */
#  define O_ASYNC		0x0040	/* Signal-driven I/O */
#  define O_FSYNC		0x0080	/* Synchronous writes */
#  define O_SYNC		O_FSYNC
#  define O_NOFOLLOW	0x0100	/* Do not follow symlinks */
#  define O_CREAT		0x0200	/* Create file if it does not exist */
#  define O_TRUNC		0x0400	/* Truncate file to zero length */
#  define O_EXCL		0x0800	/* Fail if O_CREAT and file exists */
#  define O_NOCTTY		0x8000	/* Do not assign controlling terminal */
#  define O_DIRECT		0x00010000	/* Direct I/O */
#  define O_DIRECTORY	0x00020000	/* Must be a directory */
#  define O_EXEC		0x00040000	/* Execute-only */
#  define O_TTY_INIT	0x00080000	/* Initialize terminal */
#  define O_CLOEXEC		0x00100000	/* Close on exec */

/*
 * fcntl() commands.
 */

#  define F_DUPFD		0
#  define F_GETFD		1
#  define F_SETFD		2
#  define F_GETFL		3
#  define F_SETFL		4
#  define F_GETOWN		5
#  define F_SETOWN		6
#  define F_GETLK		7
#  define F_SETLK		8
#  define F_SETLKW		9
#  define F_DUPFD_CLOEXEC 17

/*
 * File descriptor flags.
 */

#  define FD_CLOEXEC	1

/*
 * Lock types.
 */

#  define F_RDLCK		1
#  define F_UNLCK		2
#  define F_WRLCK		3

/* ----- Darwin (macOS, iOS) ----- */

# elif defined(HAJ_OS_DARWIN)

/*
 * open() flags. Values are in hexadecimal.
 */

#  define O_RDONLY		0x0000	/* Open for reading only */
#  define O_WRONLY		0x0001	/* Open for writing only */
#  define O_RDWR		0x0002	/* Open for reading and writing */
#  define O_ACCMODE		0x0003	/* Mask for access mode */

#  define O_NONBLOCK	0x0004	/* Non-blocking mode */
#  define O_APPEND		0x0008	/* Append mode */
#  define O_SHLOCK		0x0010	/* Shared lock */
#  define O_EXLOCK		0x0020	/* Exclusive lock */
#  define O_ASYNC		0x0040	/* Signal-driven I/O */
#  define O_FSYNC		0x0080	/* Synchronous writes */
#  define O_SYNC		O_FSYNC
#  define O_NOFOLLOW	0x0100	/* Do not follow symlinks */
#  define O_CREAT		0x0200	/* Create file if it does not exist */
#  define O_TRUNC		0x0400	/* Truncate file to zero length */
#  define O_EXCL		0x0800	/* Fail if O_CREAT and file exists */
#  define O_NOCTTY		0x20000	/* Do not assign controlling terminal */
#  define O_DIRECTORY	0x100000	/* Must be a directory */
#  define O_CLOEXEC		0x1000000	/* Close on exec */

/*
 * fcntl() commands.
 *
 * Darwin uses a different numbering for F_* commands than Linux
 * and FreeBSD. This is a well-known portability trap.
 */

#  define F_DUPFD		0
#  define F_GETFD		1
#  define F_SETFD		2
#  define F_GETFL		3
#  define F_SETFL		4
#  define F_GETOWN		5
#  define F_SETOWN		6
#  define F_GETLK		7
#  define F_SETLK		8
#  define F_SETLKW		9
#  define F_DUPFD_CLOEXEC 67

/*
 * File descriptor flags.
 */

#  define FD_CLOEXEC	1

/*
 * Lock types.
 */

#  define F_RDLCK		1
#  define F_UNLCK		2
#  define F_WRLCK		3

/* ----- Windows (mingw CRT) ----- */

# elif defined(HAJ_OS_WINDOWS)

/*
 * Windows CRT open() flags. These are different from the Unix
 * values. The mingw CRT provides both _O_* and O_* names.
 *
 * Windows does not have the full fcntl() API. The few functions
 * that exist (locking) use different constants.
 */

#  define O_RDONLY		_O_RDONLY
#  define O_WRONLY		_O_WRONLY
#  define O_RDWR		_O_RDWR
#  define O_ACCMODE		_O_ACCMODE

#  define O_CREAT		_O_CREAT
#  define O_EXCL		_O_EXCL
#  define O_TRUNC		_O_TRUNC
#  define O_APPEND		_O_APPEND
#  define O_NOINHERIT	_O_NOINHERIT
#  define O_TEXT		_O_TEXT
#  define O_BINARY		_O_BINARY
#  define O_RAW			_O_RAW
#  define O_TEMPORARY	_O_TEMPORARY
#  define O_SEQUENTIAL	_O_SEQUENTIAL
#  define O_RANDOM		_O_RANDOM

/*
 * Windows does not define fcntl() commands in the same way.
 * We define the POSIX names as no-ops for compatibility.
 */

#  define F_DUPFD		0
#  define F_GETFD		1
#  define F_SETFD		2
#  define F_GETFL		3
#  define F_SETFL		4
#  define F_GETLK		5
#  define F_SETLK		6
#  define F_SETLKW		7

#  define FD_CLOEXEC	1

#  define F_RDLCK		0
#  define F_WRLCK		1
#  define F_UNLCK		2

/* ----- Unknown ----- */

# else

/*
 * Fallback: use the Linux-like values. This is a best-effort for
 * unsupported OSes. If you port hajlib to a new OS, add a proper
 * block above.
 */

#  define O_RDONLY		00000000
#  define O_WRONLY		00000001
#  define O_RDWR		00000002
#  define O_ACCMODE		00000003
#  define O_CREAT		00000100
#  define O_EXCL		00000200
#  define O_TRUNC		00001000
#  define O_APPEND		00002000
#  define O_NONBLOCK	00004000
#  define O_CLOEXEC		02000000

#  define F_DUPFD		0
#  define F_GETFD		1
#  define F_SETFD		2
#  define F_GETFL		3
#  define F_SETFL		4
#  define F_GETLK		5
#  define F_SETLK		6
#  define F_SETLKW		7

#  define FD_CLOEXEC	1

#  define F_RDLCK		0
#  define F_WRLCK		1
#  define F_UNLCK		2

# endif

#endif /* _BITS_FCNTL_H */
