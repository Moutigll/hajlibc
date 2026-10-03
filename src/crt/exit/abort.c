/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file abort.c
 * @brief Implementation of abort().
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/10/03 15:37:35 by Moutig
 *
 * abort() raises SIGABRT on the calling thread. If the signal
 * is caught and the handler returns, or if SIGABRT is ignored,
 * abort() falls back to terminating the process with _exit,
 * using a non-zero status.
 *
 * It never returns to the caller and does not run atexit
 * handlers, TSD destructors, or flush stdio.
 */

#include <signal.h>
#include <unistd.h>
#include <bits/syscall.h>
#include <bits/thread/thread.h>

__HAJ_NORETURN
void abort(void)
{
	sigset_t	set;

	/*
	 * Step 1: unblock SIGABRT. If the caller blocked it, the
	 * raise below would just queue the signal, and we'd fall
	 * through to _exit. POSIX requires us to unblock it.
	 */
	sigemptyset(&set);
	sigaddset(&set, SIGABRT);
	sigprocmask(SIG_UNBLOCK, &set, NULL);

	/*
	 * Step 2: raise SIGABRT. In a multithreaded process, this
	 * must target the calling thread (tgkill), not the whole
	 * process (kill), otherwise the signal could be delivered
	 * to another thread. raise() does this for us.
	 */
	raise(SIGABRT);

	/*
	 * Step 3: if we are still here, the handler returned or
	 * SIGABRT was ignored. Reset SIGABRT to SIG_DFL and raise
	 * again, so the default action (terminate) takes effect.
	 */
	signal(SIGABRT, SIG_DFL);
	raise(SIGABRT);

	/*
	 * Step 4: last resort. If the second raise failed (for
	 * example because the syscall failed), terminate the
	 * process directly with a non-zero status.
	 */
	_exit(127);
}
