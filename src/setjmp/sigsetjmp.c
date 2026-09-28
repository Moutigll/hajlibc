/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file sigsetjmp.c
 * @brief Signal mask handling for sigsetjmp and siglongjmp.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 18:30:59 by Moutig
 *
 * The register save/restore is done in assembly (setjmp.S and
 * longjmp.S). This file handles the signal mask, which requires
 * a syscall.
 *
 * __haj_sigsetjmp_save is called by __haj_sigsetjmp (assembly)
 * when the savesigs argument is non-zero. It saves the current
 * signal mask into env->__saved_mask and sets
 * env->__mask_saved = 1.
 *
 * __haj_siglongjmp_restore is called by siglongjmp (below) to
 * restore the saved signal mask before the jump.
 *
 * For now, the signal mask handling is a stub: we set
 * __mask_saved to 0 and do not call sigprocmask. This is
 * sufficient for programs that do not use signals. The full
 * implementation will be added when the signal subsystem is
 * available.
 */

#include <setjmp.h>

void	__haj_sigsetjmp_save(struct __haj_jmp_buf *env)
{
	/**
	 * @TODO: call sigprocmask(SIG_SETMASK, NULL,
	 *                       (sigset_t *)env->__saved_mask)
	 * and set env->__mask_saved = 1 on success.
	 *
	 * For now, we do not save the mask.
	 */
	env->__mask_saved = 0;
}

/**
 * @brief Restore the signal mask from env.
 *
 * Called by siglongjmp below when env->__mask_saved != 0.
 *
 * @param env The jmp_buf containing the saved mask.
 */
static void	__haj_siglongjmp_restore(struct __haj_jmp_buf *env)
{
	/**
	 * @TODO: call sigprocmask(SIG_SETMASK,
	 *                       (const sigset_t *)env->__saved_mask,
	 *                       NULL).
	 *
	 * For now, this is a no-op.
	 */
	(void)env;
}

__HAJ_NORETURN
void	siglongjmp(sigjmp_buf env, int val)
{
	if (env[0].__mask_saved != 0)
		__haj_siglongjmp_restore(&env[0]);

	longjmp(env, val);
}
