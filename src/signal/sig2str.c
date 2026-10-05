/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file sig2str.c
 * @brief Maps signal numbers to names and vice versa.
 * @Created: 2026/10/03 14:57:36 by Moutig
 * @Updated: 2026/10/03 15:10:58 by Moutig
 *
 * sig2str() maps a signal number to its name (without the
 * "SIG" prefix). str2sig() is the reverse. The name table
 * mirrors <bits/signal.h>.
 */

#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

static const char *const g_sigNames[] = {
	[0]			= "EXIT",
	[SIGHUP]	= "HUP",
	[SIGINT]	= "INT",
	[SIGQUIT]	= "QUIT",
	[SIGILL]	= "ILL",
	[SIGTRAP]	= "TRAP",
	[SIGABRT]	= "ABRT",
	[SIGBUS]	= "BUS",
	[SIGFPE]	= "FPE",
	[SIGKILL]	= "KILL",
	[SIGUSR1]	= "USR1",
	[SIGSEGV]	= "SEGV",
	[SIGUSR2]	= "USR2",
	[SIGPIPE]	= "PIPE",
	[SIGALRM]	= "ALRM",
	[SIGTERM]	= "TERM",
	[SIGSTKFLT]	= "STKFLT",
	[SIGCHLD]	= "CHLD",
	[SIGCONT]	= "CONT",
	[SIGSTOP]	= "STOP",
	[SIGTSTP]	= "TSTP",
	[SIGTTIN]	= "TTIN",
	[SIGTTOU]	= "TTOU",
	[SIGURG]	= "URG",
	[SIGXCPU]	= "XCPU",
	[SIGXFSZ]	= "XFSZ",
	[SIGVTALRM]	= "VTALRM",
	[SIGPROF]	= "PROF",
	[SIGWINCH]	= "WINCH",
	[SIGIO]		= "IO",
	[SIGPWR]	= "PWR",
	[SIGSYS]	= "SYS",
};

int sig2str(int signum, char *buf)
{
	const char	*name;
	size_t		len;
	size_t		i;

	if (buf == NULL) {
		errno = EINVAL;
		return (-1);
	}
	if (signum < 0 || signum > SIGSYS) {
		/* Real-time signal? */
		if (signum >= SIGRTMIN && signum <= SIGRTMAX) {
			char	tmp[8];
			int		n = signum - SIGRTMIN;
			int		j = 0;

			/* "RTMIN", "RTMIN+1", ... */
			if (n == 0) {
				memcpy(buf, "RTMIN", 6);
				return (0);
			}
			tmp[j++] = '+';
			if (n >= 10)
				tmp[j++] = (char)('0' + n / 10);
			tmp[j++] = (char)('0' + n % 10);
			tmp[j] = '\0';
			memcpy(buf, "RTMIN", 5);
			memcpy(buf + 5, tmp, (size_t)j + 1);
			return (0);
		}
		errno = EINVAL;
		return (-1);
	}

	name = g_sigNames[signum];
	if (name == NULL) {
		errno = EINVAL;
		return (-1);
	}
	len = 0;
	while (name[len])
		len++;
	if (len + 1 > SIG2STR_MAX) {
		errno = EINVAL;
		return (-1);
	}
	for (i = 0; i < len; i++)
		buf[i] = name[i];
	buf[len] = '\0';
	return (0);
}

int str2sig(const char *str, int *signum)
{
	size_t	len;
	int		i;

	if (str == NULL || signum == NULL) {
		errno = EINVAL;
		return (-1);
	}
	/* Skip optional "SIG" prefix. */
	if (strncmp(str, "SIG", 3) == 0)
		str += 3;

	len = 0;
	while (str[len] && str[len] != '+')
		len++;

	for (i = 1; i <= SIGSYS; i++) {
		const char	*name = g_sigNames[i];
		size_t		nlen;

		if (name == NULL)
			continue;
		nlen = 0;
		while (name[nlen])
			nlen++;
		if (nlen != len)
			continue;
		if (memcmp(name, str, len) == 0) {
			*signum = i;
			return (0);
		}
	}

	/* Real-time signal. */
	if (len == 5 && strncmp(str, "RTMIN", 5) == 0) {
		if (str[5] == '\0') {
			*signum = SIGRTMIN;
			return (0);
		}
		if (str[5] == '+' && isdigit(str[6])) {
			int n = str[6] - '0';

			if (isdigit(str[7])) {
				n = n * 10 + (str[7] - '0');
				if (str[8] != '\0') {
					errno = EINVAL;
					return (-1);
				}
			} else if (str[7] != '\0') {
				errno = EINVAL;
				return (-1);
			}
			if (n > SIGRTMAX - SIGRTMIN) {
				errno = EINVAL;
				return (-1);
			}
			*signum = SIGRTMIN + n;
			return (0);
		}
	}
	errno = EINVAL;
	return (-1);
}
