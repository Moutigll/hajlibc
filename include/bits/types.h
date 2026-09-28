/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file types.h
 * @brief Internal fixed-width and POSIX-like types.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/28 08:10:17 by Moutig
 *
 * This header defines the real typedefs used across hajlib: the
 * fixed-width integer types, the size-related types, and the
 * POSIX types. Public headers (stddef.h, sys/types.h, time.h, ...)
 * expose them by including this header; they do not define them
 * themselves.
 *
 * Naming convention:
 *   - Internal types use the __haj_ prefix (e.g. __haj_i64,
 *     __haj_size, __haj_mode).
 *   - Public types use their POSIX/C name (size_t, mode_t,
 *     time_t, ...) and are defined here, guarded so that a
 *     system header cannot redefine them.
 *
 * Do NOT include this header directly from user code. Use the
 * public headers (stddef.h, sys/types.h, time.h, ...) instead.
 */

#ifndef _BITS_TYPES_H
# define _BITS_TYPES_H

# include <bits/os.h>
# include <bits/arch.h>
# include <bits/wordsize.h>
# include <bits/compiler.h>

/* ----- Fixed-width integer types ----- */
/**
 * We could use the compiler's __INT8_TYPE__ and friends, but we
 * define them explicitly for clarity. All modern platforms use
 * two's complement, so signed/unsigned char, short, int, long
 * have the expected sizes.
 */


typedef signed char			__haj_i8;
typedef unsigned char		__haj_u8;

typedef short				__haj_i16;
typedef unsigned short		__haj_u16;

typedef int					__haj_i32;
typedef unsigned int		__haj_u32;

# if __HAJ_SIZEOF_LONG == 8
typedef long				__haj_i64;
typedef unsigned long		__haj_u64;
# elif __HAJ_SIZEOF_LONG_LONG == 8
typedef long long			__haj_i64;
typedef unsigned long long	__haj_u64;
# else
#  error "hajlib: no 64-bit integer type available"
# endif

/*
 * Pointer-sized integer types.
 * intptr_t is signed, uintptr_t is unsigned.
 * Both have the same size as a pointer.
 */
# if __HAJ_SIZEOF_POINTER == 8
typedef __haj_i64			__haj_intptr;
typedef __haj_u64			__haj_uintptr;
# else
typedef __haj_i32			__haj_intptr;
typedef __haj_u32			__haj_uintptr;
# endif

/*
 * Maximum-width integer types.
 * intmax_t and uintmax_t are the widest integer types available.
 */
typedef __haj_i64			__haj_intmax;
typedef __haj_u64			__haj_uintmax;

/* ----- Size-related types ----- */
/**
 * size_t   : unsigned, result of sizeof
 * ssize_t  : signed, size of a buffer, return of read/write
 * ptrdiff_t: signed, result of pointer subtraction
 *
 * size_t and ptrdiff_t are the same width as a pointer on every
 * modern platform. ssize_t is the signed counterpart of size_t.
 */

# if __HAJ_WORDSIZE == 64
typedef unsigned long		__haj_size;
typedef long				__haj_ssize;
typedef long				__haj_ptrdiff;
# else
typedef unsigned int		__haj_size;
typedef int					__haj_ssize;
typedef int					__haj_ptrdiff;
# endif

/* ----- POSIX types ----- */
/**
 * These match the kernel ABI per OS. The sizes are chosen to match
 * what the kernel expects when passing these values to syscalls.
 */

/*
 * mode_t: file mode (permissions + file type bits).
 *
 * Linux  : unsigned int (32 bits) on all arches.
 * FreeBSD: unsigned short (16 bits) on most arches.
 * Darwin : unsigned short (16 bits).
 * Windows: not used (no POSIX mode in the kernel ABI).
 *
 * We use the kernel's width so that direct syscalls with a mode_t
 * argument pass the correct value.
 */
# if defined(HAJ_OS_LINUX)
typedef unsigned int		__haj_mode;
# elif defined(HAJ_OS_FREEBSD) || defined(HAJ_OS_DARWIN)
typedef unsigned short		__haj_mode;
# else
typedef unsigned int		__haj_mode;
# endif

/*
 * dev_t: device ID.
 * 64 bits unsigned on modern Linux and FreeBSD.
 * 32 bits signed on Darwin (historical).
 */
# if defined(HAJ_OS_DARWIN)
typedef __haj_i32			__haj_dev;
# else
typedef __haj_u64			__haj_dev;
# endif

/*
 * nlink_t: link count.
 *
 * Linux/FreeBSD : 64 bits unsigned.
 * Darwin        : 16 bits unsigned.
 *
 * Using the correct width is required for struct stat layout
 * on Darwin. If you never call stat on Darwin, you can use
 * 64 bits everywhere for simplicity.
 */
# if defined(HAJ_OS_DARWIN)
typedef unsigned short __haj_nlink;
# else
typedef __haj_u64 __haj_nlink;
# endif

/**
 * __hajULW_t: an unaligned word type.
 * This type is used to represent a word that is not aligned to its natural
 * boundary. It is typically used in low-level memory operations where
 * alignment is not guaranteed.
 */
typedef __haj_size __HAJ_UNALIGNED_WORD __hajULW_t;





/* ----- Public types ----- */


/* ----- Size-related types ----- */

# ifndef __size_t_defined
#  define __size_t_defined
/**
 * @brief Unsigned integer type of the result of the sizeof operator.
 */
typedef __haj_size		size_t;
# endif

# ifndef __ssize_t_defined
#  define __ssize_t_defined
/**
 * @brief Signed integer type, used for sizes and counts.
 *
 * Used by read(), write(), and other functions that return a
 * byte count or -1 on error.
 */
typedef __haj_ssize		ssize_t;
# endif

# ifndef __ptrdiff_t_defined
#  define __ptrdiff_t_defined
/**
 * @brief Signed integer type of the result of subtracting two pointers.
 */
typedef __haj_ptrdiff	ptrdiff_t;
# endif

/* ----- POSIX types ----- */

# ifndef __mode_t_defined
#  define __mode_t_defined
/**
 * @brief File mode (permissions and file type).
 *
 * Used by open(), mkdir(), chmod(), and struct stat.
 */
typedef __haj_mode		mode_t;
# endif

# ifndef __off_t_defined
#  define __off_t_defined
/**
 * @brief File offset.
 *
 * Used by lseek(), mmap(), and struct stat.
 * 64 bits on all modern 64-bit platforms.
 * The user can force 32 bits with -D_FILE_OFFSET_BITS=32.
 */
#  if __HAJ_USE_32_OFFSET_BITS
typedef __haj_i32		off_t;
#  else
typedef __haj_i64		off_t;
#  endif
# endif

# ifndef __pid_t_defined
#  define __pid_t_defined
/**
 * @brief Process ID.
 */
typedef int				pid_t;
# endif

# ifndef __uid_t_defined
#  define __uid_t_defined
/**
 * @brief User ID.
 */
typedef unsigned int	uid_t;
# endif

# ifndef __gid_t_defined
#  define __gid_t_defined
/**
 * @brief Group ID.
 */
typedef unsigned int	gid_t;
# endif

# ifndef __dev_t_defined
#  define __dev_t_defined
/**
 * @brief Device ID.
 *
 * Used by mknod(), struct stat, and struct dirent.
 */
typedef __haj_dev		dev_t;
# endif

# ifndef __ino_t_defined
#  define __ino_t_defined
/**
 * @brief Inode number.
 *
 * Used by struct stat and struct dirent.
 */
#  if __HAJ_USE_32_OFFSET_BITS
typedef __haj_u32		ino_t;
#  else
typedef __haj_u64		ino_t;
#  endif
# endif

# ifndef __nlink_t_defined
#  define __nlink_t_defined
/**
 * @brief Link count.
 *
 * Used by struct stat.
 */
typedef __haj_nlink		nlink_t;
# endif

# ifndef __blksize_t_defined
#  define __blksize_t_defined
/**
 * @brief Block size.
 *
 * Used by struct stat.
 */
typedef long		blksize_t;
# endif

# ifndef __blkcnt_t_defined
#  define __blkcnt_t_defined
/**
 * @brief Block count.
 *
 * Used by struct stat.
 */
#  if __HAJ_USE_32_OFFSET_BITS
typedef __haj_i32		blkcnt_t;
#  else
typedef __haj_i64		blkcnt_t;
#  endif
# endif

# ifndef __time_t_defined
#  define __time_t_defined
/**
 * @brief Calendar time in seconds since the Unix epoch.
 *
 * Used by time(), struct stat, struct timespec, struct timeval.
 */
typedef __haj_i64		time_t;
# endif

# ifndef __suseconds_t_defined
#  define __suseconds_t_defined
/**
 * @brief Microseconds.
 *
 * Used by struct timeval.
 */
typedef long			suseconds_t;
# endif

# ifndef __clock_t_defined
#  define __clock_t_defined
/**
 * @brief Clock ticks.
 *
 * Used by clock().
 */
typedef long			clock_t;
# endif

# ifndef __clockid_t_defined
#  define __clockid_t_defined
/**
 * @brief Clock ID.
 *
 * Used by clock_gettime(), clock_settime(), etc.
 */
typedef int			clockid_t;
# endif

#endif /* _BITS_TYPES_H */
