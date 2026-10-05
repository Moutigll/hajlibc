/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file signal.h
 * @brief Signal handling functions and types.
 * @Created: 2026/10/03 14:54:05 by Moutig
 * @Updated: 2026/10/03 16:15:52 by Moutig
 *
 * This header provides the POSIX signal handling API, including
 * sigaction(), signal(), sigprocmask(), and related functions.
 * It defines the sigset_t type, signal numbers, and constants.
 */


#ifndef _SIGNAL_H
# define _SIGNAL_H

# include <bits/signal.h>
# include <bits/time.h>

# if defined(__cplusplus)
extern "C" {
# endif

/* ----- sigset_t operations ----- */

/**
 * @brief Initialize a signal set to be empty.
 * @param set The set to initialize.
 * @return 0 on success, -1 with errno on failure.
 */
int sigemptyset(sigset_t *set);

/**
 * @brief Initialize a signal set to contain every signal.
 * @param set The set to initialize.
 * @return 0 on success, -1 with errno on failure.
 */
int sigfillset(sigset_t *set);

/**
 * @brief Add a signal to a signal set.
 * @param set    The set to modify.
 * @param signum The signal to add.
 * @return 0 on success, -1 with errno on failure.
 */
int sigaddset(sigset_t *set, int signum);

/**
 * @brief Remove a signal from a signal set.
 * @param set    The set to modify.
 * @param signum The signal to remove.
 * @return 0 on success, -1 with errno on failure.
 */
int sigdelset(sigset_t *set, int signum);

/**
 * @brief Test whether a signal is in a signal set.
 * @param set    The set to query.
 * @param signum The signal to test.
 * @return 1 if present, 0 if not, -1 with errno on failure.
 */
int sigismember(const sigset_t *set, int signum);

/* ----- Handler installation ----- */

/**
 * @brief Install or query a signal handler.
 *
 * @param signum The signal number.
 * @param act    New disposition, or NULL to query only.
 * @param oldact Previous disposition, or NULL to discard.
 * @return 0 on success, -1 with errno on failure.
 */
int sigaction(int signum, const struct sigaction *act,
			  struct sigaction *oldact);

/**
 * @brief Install a signal handler (ANSI C).
 *
 * @param signum  The signal number.
 * @param handler SIG_DFL, SIG_IGN, or a function.
 * @return The previous handler on success, SIG_ERR on failure.
 */
void (*signal(int signum, void (*handler)(int)))(int);

/* ----- Process signal mask ----- */

/**
 * @brief Change the calling process' signal mask (POSIX, single-threaded).
 *
 * @param how    SIG_BLOCK, SIG_UNBLOCK, SIG_SETMASK.
 * @param set    The set to apply, or NULL to query.
 * @param oldset Output: previous mask, or NULL to discard.
 * @return 0 on success, -1 with errno on failure.
 */
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);

/**
 * @brief Get the set of signals pending for the calling process or thread.
 * @param set Output: the pending set.
 * @return 0 on success, -1 with errno on failure.
 */
int sigpending(sigset_t *set);

/**
 * @brief Atomically replace the signal mask and suspend the thread.
 * @param mask The new mask to install while suspended.
 * @return Always returns -1 with errno == EINTR.
 */
int sigsuspend(const sigset_t *mask);

/* ----- Alternate signal stack ----- */

/**
 * @brief Install or query an alternate signal stack.
 * @param ss     New stack, or NULL to query.
 * @param old_ss Previous stack, or NULL to discard.
 * @return 0 on success, -1 with errno on failure.
 */
int sigaltstack(const stack_t *ss, stack_t *old_ss);

/* ----- Sending signals ----- */

/**
 * @brief Send a signal to the calling thread.
 * @param signum The signal number.
 * @return 0 on success, -1 with errno on failure.
 */
int raise(int signum);

/**
 * @brief Send a signal to a process.
 * @param pid    Target PID.
 * @param signum Signal number.
 * @return 0 on success, -1 with errno on failure.
 */
int kill(pid_t pid, int signum);

/**
 * @brief Send a signal to a specific thread of a process.
 * @param tgid   Target process ID (thread group).
 * @param tid    Target thread ID.
 * @param signum Signal number.
 * @return 0 on success, -1 with errno on failure.
 */
int tgkill(pid_t tgid, pid_t tid, int signum);

/**
 * @brief Send a signal to a process group.
 * @param pgrp   Target process group ID.
 * @param signum Signal number.
 * @return 0 on success, -1 with errno on failure.
 */
int killpg(pid_t pgrp, int signum);

/**
 * @brief Send a signal with an accompanying value to a process.
 * @param pid    Target PID.
 * @param signum Signal number.
 * @param value  Value to send with the signal.
 * @return 0 on success, -1 with errno on failure.
 */
int sigqueue(pid_t pid, int signum, const union sigval value);

/* ----- Signal waiting ----- */

/**
 * @brief Wait for a signal to be delivered, with an optional timeout.
 * @param set     Set of signals to wait for.
 * @param info    Output: information about the received signal.
 * @param timeout Optional timeout, or NULL for indefinite wait.
 * @return The signal number received, or -1 with errno on failure.
 */
int sigtimedwait(const sigset_t *set, siginfo_t *info, const struct timespec *timeout);

/**
 * @brief Wait for a signal to be delivered, without a timeout.
 * @param set  Set of signals to wait for.
 * @param info Output: information about the received signal.
 * @return The signal number received, or -1 with errno on failure.
 */
int sigwaitinfo(const sigset_t *set, siginfo_t *info);

/* ----- Thread-specific signal handling ----- */

/**
 * @brief Send a signal to a specific thread in the same process.
 * @param thread The target pthread_t.
 * @param signum The signal number.
 * @return 0 on success, or an error number on failure.
 */
int pthread_kill(pthread_t thread, int signum);

/**
 * @brief Change the calling thread's signal mask.
 * @param how    SIG_BLOCK, SIG_UNBLOCK, SIG_SETMASK.
 * @param set    The set to apply, or NULL to query.
 * @param oldset Output: previous mask, or NULL to discard.
 * @return 0 on success, or an error number on failure.
 */
int pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset);

/**
 * @brief Wait for a signal to be delivered to the calling thread.
 * @param set  Set of signals to wait for.
 * @param sig  Output: the signal number received.
 * @return 0 on success, or an error number on failure.
 */
int sigwait(const sigset_t *set, int *sig);

/* ----- Signal names ----- */

/**
 * @brief Get the name of a signal.
 * @param signum The signal number.
 * @param buf    Output buffer of at least SIG2STR_MAX bytes.
 * @return 0 on success, -1 with errno on failure.
 */
int sig2str(int signum, char *buf);

/**
 * @brief Parse a signal name.
 * @param str    The signal name.
 * @param signum Output: the signal number.
 * @return 0 on success, -1 with errno on failure.
 */
int str2sig(const char *str, int *signum);

/**
 * @brief Get a string describing a signal number.
 * @param signum The signal number.
 * @return A string describing the signal, or "Unknown signal" if invalid.
 */
char *strsignal(int signum);

/**
 * @brief Print a signal description to stderr, optionally prefixed.
 * @param signum The signal number.
 * @param s      Optional prefix string, or NULL.
 */
void psignal(int signum, const char *s);

/**
 * @brief Print a signal description from a siginfo_t to stderr, optionally prefixed.
 * @param info   The siginfo_t structure.
 * @param s      Optional prefix string, or NULL.
 */
void psiginfo(const siginfo_t *info, const char *s);

# if defined(__cplusplus)
}
# endif

#endif /* _SIGNAL_H */
