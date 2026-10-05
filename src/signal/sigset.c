/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file sigset.c
 * @brief Implementation of sigemptyset(), sigfillset(), sigaddset(), sigdelset() and sigismember().
 * @Created: 2026/10/03 14:55:25 by Moutig
 * @Updated: 2026/10/03 16:26:52 by Moutig
 *
 * We use a single unsigned long per set, giving room for 64
 * signals. Linux' NSIG is 64. Signals are numbered starting at
 * 1, and are stored in bit (signum - 1).
 *
 * All functions validate the signal number and return EINVAL
 * for anything outside [1, 64].
 */

#include <signal.h>
#include <errno.h>
#include <string.h>

static __HAJ_INLINE int validSig(int signum)
{
	return (signum >= 1 && signum <= HAJ_NSIG);
}

int sigemptyset(sigset_t *set)
{
	if (set == NULL) {
		errno = EINVAL;
		return (-1);
	}
	memset(set, 0, sizeof(*set));
	return (0);
}

int sigfillset(sigset_t *set)
{
	if (set == NULL) {
		errno = EINVAL;
		return (-1);
	}
	set->__bits[0] = ~0UL;
	return (0);
}

int sigaddset(sigset_t *set, int signum)
{
	if (set == NULL || !validSig(signum)) {
		errno = EINVAL;
		return (-1);
	}
	set->__bits[0] |= 1UL << (signum - 1);
	return (0);
}

int sigdelset(sigset_t *set, int signum)
{
	if (set == NULL || !validSig(signum)) {
		errno = EINVAL;
		return (-1);
	}
	set->__bits[0] &= ~(1UL << (signum - 1));
	return (0);
}

int sigismember(const sigset_t *set, int signum)
{
	if (set == NULL || !validSig(signum)) {
		errno = EINVAL;
		return (-1);
	}
	return ((set->__bits[0] & (1UL << (signum - 1))) != 0);
}
