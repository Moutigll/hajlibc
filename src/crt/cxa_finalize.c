/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file cxa_finalize.c
 * @brief Implementation of __cxa_finalize.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 18:25:59 by Moutig
 *
 * __cxa_finalize is called when a shared library is unloaded, or
 * when the process exits, to run all destructors registered with
 * __cxa_atexit for that DSO.
 *
 * Since we do not use shared libraries, this is mostly a no-op.
 * It is provided for compatibility with code compiled with
 * GCC/Clang that generates references to it.
 */

#include <stddef.h>

#include <bits/crt.h>

/*
 * When dso_handle is NULL, run all handlers. Otherwise, run only
 * the handlers registered for that specific DSO. We do not track
 * DSOs yet, so we only support the NULL case.
 */

void	__cxa_finalize(void *dso_handle)
{
	/*
	 * When dso_handle is NULL, run all handlers. Otherwise,
	 * run only the handlers registered for that specific DSO.
	 *
	 * We do not track DSOs, so we only support the NULL case.
	 * This is the case that matters for a program without
	 * shared libraries.
	 */
	if (dso_handle == NULL)
		__haj_run_cxa_atexit();
}
