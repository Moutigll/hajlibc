/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file clone.c
 * @brief Implementation of __haj_clone() with clone3(2) fallback.
 * @Created: 2026/10/01 14:33:26 by Moutig
 * @Updated: 2026/10/02 10:59:26 by Moutig
 *
 *  On the first call, we try clone3(2). If the running kernel
 * does not implement it (ENOSYS), we fall back to clone(2) and
 * remember the choice for the process lifetime.
 *
 * The cache is a plain int. The only consequence of a race is
 * that two threads might each do the initial check in parallel;
 * both will agree on the result.
 *
 * If clone3 succeeds, we cache that too, so subsequent calls
 * skip the try entirely.
 *
 * The raw wrappers are provided by clone_x86_64.S (or
 * clone_aarch64.S) and share the same C-level signature:
 *
 *   long __haj_clone3Raw  (fn, stack, flags, arg, ptid, tls, ctid);
 *   long __haj_cloneLegacyRaw(fn, stack, flags, arg, ptid, tls, ctid);
 *
 * Return values:
 *   > 0   child TID, in the parent
 *     0   we are in the child (never reaches the caller here)
 *   < 0   -errno, in the parent
 */

#include <bits/errno.h>
#include <bits/thread/thread.h>
#include <bits/thread/clone.h>

#if defined(HAJ_OS_LINUX)

long __haj_clone(int (*fn)(void *), void *stack, int flags, void *arg, int *ptid, void *tls, int *ctid)
{
	long		r;
	static int	hajUseClone3 = -1;

	if (hajUseClone3 != 0) {
		r = __haj_clone3Raw(fn, stack, flags, arg, ptid, tls, ctid);

		/*
		 * The kernel does not implement clone3. Disable it
		 * for the rest of the process lifetime and fall
		 * through to clone(2).
		 *
		 * A NULL "no TLS" is not an error for clone3: we
		 * still cache the result on success.
		 */
		if (r == -ENOSYS)
			hajUseClone3 = 0;
		else {
			hajUseClone3 = 1;
			return (r);
		}
	}

	return (__haj_cloneLegacyRaw(fn, stack, flags, arg, ptid, tls, ctid));
}

#else
# error "clone.c: only Linux is supported by this file"
#endif
