/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file psignal.c
 * @brief Implementation of strsignal(), psignal() and psiginfo().
 * @Created: 2026/10/03 15:56:00 by Moutig
 * @Updated: 2026/10/03 15:57:38 by Moutig
 *
 * strsignal() returns a string describing the signal number.
 * psignal() prints the signal description to stderr, optionally
 * prefixed by a user-provided string. psiginfo() prints the signal
 * description from a siginfo_t, optionally prefixed by a user-provided string.
 */


#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static const char *const g_sigDesc[] = {
	[SIGHUP]	= "Hangup",
	[SIGINT]	= "Interrupt",
	[SIGQUIT]	= "Quit",
	[SIGILL]	= "Illegal instruction",
	[SIGTRAP]	= "Trace/breakpoint trap",
	[SIGABRT]	= "Aborted",
	[SIGBUS]	= "Bus error",
	[SIGFPE]	= "Floating point exception",
	[SIGKILL]	= "Killed",
	[SIGUSR1]	= "User defined signal 1",
	[SIGSEGV]	= "Segmentation fault",
	[SIGUSR2]	= "User defined signal 2",
	[SIGPIPE]	= "Broken pipe",
	[SIGALRM]	= "Alarm clock",
	[SIGTERM]	= "Terminated",
	[SIGSTKFLT]	= "Stack fault",
	[SIGCHLD]	= "Child exited",
	[SIGCONT]	= "Continued",
	[SIGSTOP]	= "Stopped (signal)",
	[SIGTSTP]	= "Stopped",
	[SIGTTIN]	= "Stopped (tty input)",
	[SIGTTOU]	= "Stopped (tty output)",
	[SIGURG]	= "Urgent I/O condition",
	[SIGXCPU]	= "CPU time limit exceeded",
	[SIGXFSZ]	= "File size limit exceeded",
	[SIGVTALRM] = "Virtual timer expired",
	[SIGPROF]	= "Profiling timer expired",
	[SIGWINCH]	= "Window changed",
	[SIGIO]		= "I/O possible",
	[SIGPWR]	= "Power failure",
	[SIGSYS]	= "Bad system call",
};

char *strsignal(int signum)
{
	static char	unknown[32];

	if (signum >= 1 && signum <= SIGSYS && g_sigDesc[signum] != NULL)
		return ((char *)g_sigDesc[signum]);
	if (signum >= SIGRTMIN && signum <= SIGRTMAX) {
		snprintf(unknown, sizeof(unknown), "Real-time signal %d",
				 signum - SIGRTMIN);
		return (unknown);
	}
	snprintf(unknown, sizeof(unknown), "Unknown signal %d", signum);
	return (unknown);
}

void psignal(int signum, const char *s)
{
	if (s != NULL && *s != '\0')
		fprintf(stderr, "%s: %s\n", s, strsignal(signum));
	else
		fprintf(stderr, "%s\n", strsignal(signum));
}

void psiginfo(const siginfo_t *info, const char *s)
{
	if (info == NULL)
		return;
	psignal(info->si_signo, s);
}
