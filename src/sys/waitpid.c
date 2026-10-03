/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file waitpid.c
 * @brief TODO: brief description.
 * @Created: 2026/10/03 09:47:31 by Moutig
 * @Updated: 2026/10/03 09:50:26 by Moutig
 *
 * TODO: description.
 */

/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file waitpid.c
 * @brief Implementation of waitpid() and wait().
 * @Created: 2026/10/03 13:00:00 by Moutig
 * @Updated: 2026/10/03 09:50:26 by Moutig
 *
 * waitpid() waits for a child process to change state. The
 * kernel call is wait4() on Linux, FreeBSD, and Darwin. It
 * returns the PID of the child whose state changed, and fills
 * *status with the encoded status.
 *
 * wait() is a thin wrapper that calls waitpid(-1, status, 0).
 *
 * The status encoding is the historical Unix one, decoded by
 * the macros in <sys/wait.h>. The kernel writes it directly.
 */

#include <sys/wait.h>
#include <errno.h>
#include <bits/syscall.h>
#include <bits/os.h>

#if defined(HAJ_OS_LINUX) || defined(HAJ_OS_FREEBSD) || defined(HAJ_OS_DARWIN)

pid_t waitpid(pid_t pid, int *status, int options)
{
	long	r;

	/*
	 * wait4(pid, status, options, rusage):
	 *   arg1: pid (int)
	 *   arg2: status (int *)
	 *   arg3: options (int)
	 *   arg4: rusage (struct rusage *, unused here)
	 *
	 * The 4th argument is optional: passing 0 tells the kernel
	 * we do not want rusage information.
	 */
	r = __haj_syscall4(SYS_wait4,
					   (long)pid,
					   (long)status,
					   (long)options,
					   0);
	if (r < 0) {
		errno = (int)-r;
		return (-1);
	}
	return ((pid_t)r);
}

pid_t wait(int *status)
{
	return (waitpid(-1, status, 0));
}

#else

# error "waitpid.c: unsupported OS"

#endif
