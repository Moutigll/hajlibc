/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file fcntl.c
 * @brief Implementation of fcntl().
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/26 05:05:17 by Moutig
 *
 * fcntl() performs various operations on an open file descriptor.
 * The operation is selected by `cmd` and may take an optional
 * third argument whose type depends on `cmd`:
 *
 *   - No argument    : F_GETFD, F_GETFL, F_GETOWN, F_GETSIG, F_GETLK
 *   - int argument   : F_DUPFD, F_SETFD, F_SETFL, F_SETOWN, F_SETSIG,
 *                      F_DUPFD_CLOEXEC
 *   - struct flock * : F_SETLK, F_SETLKW
 *
 * Since some commands take an argument and others do not, fcntl()
 * is variadic. Reading the argument unconditionally is undefined
 * behavior for the commands that do not take one. We use a switch
 * to read the argument only when the command actually takes one.
 *
 * Return value: 0 or a non-negative value on success, -1 on error
 * (with errno set).
 */

#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>

#include <bits/os.h>
#include <bits/syscall.h>

/* Argument categories:
 * These macros group the fcntl commands by the type of argument
 * they expect. We use them in the switch below.
 *
 * The values come from <bits/fcntl.h> and are OS-specific.
 */

#if defined(HAJ_OS_LINUX)

/* Commands that take no argument. */
# define HAJ_FCNTL_NO_ARG(cmd)	\
	((cmd) == F_GETFD || (cmd) == F_GETFL || (cmd) == F_GETOWN \
	 || (cmd) == F_GETSIG || (cmd) == F_GETLK)

/* Commands that take an int argument. */
# define HAJ_FCNTL_INT_ARG(cmd)	\
	((cmd) == F_DUPFD || (cmd) == F_SETFD || (cmd) == F_SETFL \
	 || (cmd) == F_SETOWN || (cmd) == F_SETSIG \
	 || (cmd) == F_DUPFD_CLOEXEC)

/* Commands that take a struct flock * argument. */
# define HAJ_FCNTL_PTR_ARG(cmd)	\
	((cmd) == F_SETLK || (cmd) == F_SETLKW)

#elif defined(HAJ_OS_FREEBSD) || defined(HAJ_OS_DARWIN)

# define HAJ_FCNTL_NO_ARG(cmd)	\
	((cmd) == F_GETFD || (cmd) == F_GETFL || (cmd) == F_GETOWN \
	 || (cmd) == F_GETLK)

# define HAJ_FCNTL_INT_ARG(cmd)	\
	((cmd) == F_DUPFD || (cmd) == F_SETFD || (cmd) == F_SETFL \
	 || (cmd) == F_SETOWN || (cmd) == F_DUPFD_CLOEXEC)

# define HAJ_FCNTL_PTR_ARG(cmd)	\
	((cmd) == F_SETLK || (cmd) == F_SETLKW)

#else

/*
 * Fallback for unknown OSes: assume Linux-like commands.
 */
# define HAJ_FCNTL_NO_ARG(cmd)	\
	((cmd) == F_GETFD || (cmd) == F_GETFL || (cmd) == F_GETOWN)

# define HAJ_FCNTL_INT_ARG(cmd)	\
	((cmd) == F_DUPFD || (cmd) == F_SETFD || (cmd) == F_SETFL \
	 || (cmd) == F_SETOWN)

# define HAJ_FCNTL_PTR_ARG(cmd)	\
	((cmd) == F_SETLK || (cmd) == F_SETLKW)

#endif

int	fcntl(int fd, int cmd, ...)
{
	va_list	ap;
	long	arg = 0;
	long	ret;

	/*
	 * Read the argument only if the command expects one.
	 *
	 * Passing a command that takes a struct flock * to a
	 * function that expects an int (or vice versa) is undefined
	 * behavior. We use the classification macros defined above
	 * to read the right type.
	 *
	 * On most ABIs, a pointer and a long have the same size, so
	 * we can pass them to __haj_syscall3 as a long. This is
	 * true on all 64-bit Unix systems (LP64). On 32-bit systems
	 * (ILP32), long and pointer are both 32 bits, so it also
	 * works. On Windows 64 (LLP64), long is 32 bits but a
	 * pointer is 64 bits; we would need a separate path, but
	 * fcntl() is not used on Windows anyway.
	 */
	va_start(ap, cmd);

	if (HAJ_FCNTL_INT_ARG(cmd)) {
		/*
		 * int argument: F_DUPFD, F_SETFD, F_SETFL, F_SETOWN,
		 * F_SETSIG, F_DUPFD_CLOEXEC.
		 *
		 * The argument is an int, but we read it as a long
		 * because some commands expect a pointer-sized value
		 * (for example F_DUPFD_CLOEXEC takes an int on Linux
		 * but a long in the syscall interface).
		 */
		arg = (long)va_arg(ap, int);
	} else if (HAJ_FCNTL_PTR_ARG(cmd)) {
		/*
		 * Pointer argument: F_SETLK, F_SETLKW.
		 *
		 * The argument is a struct flock *. We pass it as a
		 * long to __haj_syscall3.
		 */
		arg = (long)(__haj_uintptr)va_arg(ap, void *);
	} else {
		/*
		 * No argument: F_GETFD, F_GETFL, F_GETOWN, F_GETSIG,
		 * F_GETLK.
		 *
		 * We do not read the argument. Doing so would be
		 * undefined behavior.
		 */
		arg = 0;
	}

	va_end(ap);

	/*
	 * Perform the syscall.
	 *
	 * On Linux, the syscall returns the raw kernel value: a
	 * non-negative value on success, or -errno in [-4095, -1]
	 * on error.
	 *
	 * On FreeBSD and Darwin, the syscall returns the value on
	 * success (carry flag clear) and errno (carry flag set) on
	 * error. The assembly wrapper normalizes this to the Linux
	 * convention.
	 */
	ret = __haj_syscall3(SYS_fcntl, fd, cmd, arg);

	if (ret < 0 && ret >= -4095) {
		errno = (int)-ret;
		return (-1);
	}

	return ((int)ret);
}
