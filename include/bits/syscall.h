/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file syscall.h
 * @brief Raw syscall interface.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/26 05:02:50 by Moutig
 *
 * This header declares __haj_syscall6 and includes the correct
 * syscall numbers for the target OS and architecture.
 *
 * The numbers themselves live in <bits/syscall/<os>/<arch>.h>.
 * See the comment at the top of each of those files for the
 * source of the numbers.
 *
 * This header is internal to hajlib; do not include it from
 * user code.
 */

#ifndef _BITS_SYSCALL_H
# define _BITS_SYSCALL_H

# include <bits/os.h>
# include <bits/arch.h>


# if defined(HAJ_OS_LINUX) && defined(HAJ_ARCH_X86_64)
#  include <bits/syscall/linux/x86_64.h>
# elif defined(HAJ_OS_LINUX) && defined(HAJ_ARCH_AARCH64)
#  include <bits/syscall/linux/aarch64.h>
# elif defined(HAJ_OS_FREEBSD)
#  include <bits/syscall/freebsd.h>
# elif defined(HAJ_OS_DARWIN)
#  include <bits/syscall/darwin.h>
# elif defined(HAJ_OS_WINDOWS)
#  include <bits/syscall/windows.h>
# else
#  error "hajlib: no syscall numbers for this OS/arch"
# endif

/*
 * AT_* flags for the *at() syscalls. Values are POSIX and are
 * the same on Linux, FreeBSD, and Darwin. Windows does not have
 * the *at() family, so these are unused there.
 */
# ifndef HAJ_AT_SYMLINK_NOFOLLOW
#  define HAJ_AT_SYMLINK_NOFOLLOW	0x0100
#  define HAJ_AT_REMOVEDIR			0x0200
#  define HAJ_AT_SYMLINK_FOLLOW		0x0400
#  define HAJ_AT_EACCESS			0x0200
#  define HAJ_AT_EMPTY_PATH			0x1000
# endif

/**
 * @brief Make a system call with up to 6 arguments.
 *
 * The exact behavior depends on the OS:
 *
 *   Linux  : returns the raw kernel value. If it is in
 *            [-4095, -1], it encodes -errno.
 *   FreeBSD: on error, sets the carry flag and returns errno
 *            (positive). We normalize to -errno.
 *   Darwin : same as FreeBSD.
 *   Windows: dispatches to the mingw CRT. This is NOT a real
 *            syscall; it is a compatibility shim.
 *
 * @param nr  Syscall number (SYS_*).
 * @param a1  First argument.
 * @param a2  Second argument.
 * @param a3  Third argument.
 * @param a4  Fourth argument.
 * @param a5  Fifth argument.
 * @param a6  Sixth argument.
 * @return The raw result. Negative values in [-4095, -1] encode
 *         -errno.
 */
long	__haj_syscall6(long nr, long a1, long a2, long a3,
					   long a4, long a5, long a6);

long	__haj_syscall5(long nr, long a1, long a2, long a3,
					   long a4, long a5);

long	__haj_syscall4(long nr, long a1, long a2, long a3,
					   long a4);

long	__haj_syscall3(long nr, long a1, long a2, long a3);

long	__haj_syscall2(long nr, long a1, long a2);

long	__haj_syscall1(long nr, long a1);

long	__haj_syscall0(long nr);

#endif /* _BITS_SYSCALL_H */
