/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file wait.h
 * @brief Wait for process termination.
 * @Created: 2026/10/03 09:47:08 by Moutig
 * @Updated: 2026/10/03 09:49:49 by Moutig
 *
 * This header declares the POSIX wait functions: wait(),
 * waitpid(), and the macros used to inspect the status
 * returned by them.
 *
 * The status encoding is the traditional Unix one:
 *   - If the process exited normally, the exit code is in
 *     bits 8-15.
 *   - If the process was killed by a signal, the signal number
 *     is in bits 0-6, and bit 7 is set.
 *   - If the process was stopped, bit 16 is set, and the stop
 *     signal is in bits 8-15.
 *   - If the process was continued, bit 16 is set (without a
 *     stop signal).
 *
 * The macros below implement exactly this encoding, matching
 * what Linux, FreeBSD, and Darwin use.
 */

#ifndef _SYS_WAIT_H
# define _SYS_WAIT_H

# include <bits/types.h>

# ifdef __cplusplus
extern "C" {
# endif

/* ----- waitpid() options ----- */

/*
 * If set, do not block if no child has exited: return
 * immediately.
 */
# define WNOHANG	1

/*
 * If set, return status for stopped children as well as for
 * terminated ones.
 */
# define WUNTRACED	2

/*
 * If set, return status for continued children as well as for
 * terminated ones. (Linux-specific, also available on FreeBSD
 * and Darwin.)
 */
# define WCONTINUED	8

/* ----- Special PID values ----- */

/*
 * Wait for any child process. This is also the behaviour of
 * wait().
 */
# define WAIT_ANY	(-1)

/*
 * Wait for any child process in the same process group as the
 * caller.
 */
# define WAIT_MYPGRP	0

/* ----- Status inspection macros ----- */

/*
 * True if the child terminated normally (by exit() or by
 * returning from main).
 */
# define WIFEXITED(status)	(((status) & 0x7F) == 0)

/*
 * True if the child was terminated by a signal.
 */
# define WIFSIGNALED(status)	(((status) & 0x7F) != 0 && \
				 ((status) & 0x7F) != 0x7F)

/*
 * True if the child was stopped by a signal (only returned when
 * WUNTRACED is set in the options).
 */
# define WIFSTOPPED(status)	(((status) & 0xFF) == 0x7F)

/*
 * True if the child was resumed by SIGCONT (only returned when
 * WCONTINUED is set in the options).
 */
# define WIFCONTINUED(status)	((status) == 0xFFFF)

/*
 * Exit code of a normally terminated child. Only valid if
 * WIFEXITED(status) is true.
 */
# define WEXITSTATUS(status)	(((status) >> 8) & 0xFF)

/*
 * Signal number that terminated the child. Only valid if
 * WIFSIGNALED(status) is true.
 */
# define WTERMSIG(status)	((status) & 0x7F)

/*
 * Signal number that stopped the child. Only valid if
 * WIFSTOPPED(status) is true.
 */
# define WSTOPSIG(status)	WEXITSTATUS(status)

/*
 * True if the child produced a core dump. Only meaningful when
 * WIFSIGNALED(status) is true.
 */
# define WCOREDUMP(status)	((status) & 0x80)

/* ----- Functions ----- */

/**
 * @brief Wait for any child process to terminate.
 *
 * @param status  Output: the status of the terminated child.
 * @return The PID of the terminated child, -1 on error (errno
 *         set), or 0 if WNOHANG was used and no child has
 *         terminated.
 */
pid_t	wait(int *status);

/**
 * @brief Wait for a specific child process to terminate.
 *
 * @param pid     The PID to wait for, or a special value:
 *                  > 0   wait for the child with this PID
 *                  = 0   wait for any child in the same process
 *                        group as the caller
 *                  < -1  wait for any child whose process group
 *                        ID is -pid
 *                  = -1  wait for any child (WAIT_ANY)
 * @param status  Output: the status of the terminated child.
 * @param options A combination of WNOHANG, WUNTRACED,
 *                WCONTINUED.
 * @return The PID of the terminated child, -1 on error (errno
 *         set), or 0 if WNOHANG was used and no child has
 *         terminated.
 */
pid_t	waitpid(pid_t pid, int *status, int options);

# ifdef __cplusplus
}
# endif

#endif /* _SYS_WAIT_H */
