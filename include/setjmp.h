/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file setjmp.h
 * @brief Non-local jumps.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 16:17:50 by Moutig
 *
 * This header defines jmp_buf, sigjmp_buf, and declares setjmp,
 * longjmp, sigsetjmp, and siglongjmp.
 *
 * The jmp_buf structure is designed to hold both the register
 * context (saved by setjmp) and the signal mask (saved by
 * sigsetjmp). This allows a single struct to be used for both
 * C-standard setjmp/longjmp and POSIX sigsetjmp/siglongjmp.
 */

#ifndef _SETJMP_H
# define _SETJMP_H

# include <bits/os.h>
# include <bits/arch.h>
# include <bits/compiler.h>

# if defined(__cplusplus)
extern "C" {
# endif

/* ----- jmp_buf layout ----- */
/**
 * The register save area is large enough for all supported archs:
 *   x86_64   : 8 slots used (rbx, rbp, r12-r15, rsp, rip)
 *   aarch64  : 21 slots used (x19-x28, fp, lr, sp, d8-d15)
 *
 * We allocate 22 slots to cover aarch64 and leave room for future
 * additions (e.g. other callee-saved registers).
 *
 * The signal mask area is 128 bytes (16 longs), which is enough
 * for Linux and FreeBSD sigset_t. Darwin uses a 4-byte sigset_t,
 * but we always allocate the larger size for portability.
 */

# define __HAJ_JMPBUF_REGS	22
# define __HAJ_JMPBUF_MASK	16

/**
 * @brief A buffer to store the execution context.
 *
 * This structure is used to save and restore the execution context
 * of a program, allowing for non-local jumps.
 */
struct __haj_jmp_buf {
	long	__regs[__HAJ_JMPBUF_REGS];
	int		__mask_saved;
	long	__saved_mask[__HAJ_JMPBUF_MASK];
};

typedef struct __haj_jmp_buf	jmp_buf[1];
typedef struct __haj_jmp_buf	sigjmp_buf[1];

/* ----- Standard C functions ----- */

/**
 * @brief Save the current execution context.
 *
 * @param env Buffer to save the context into.
 * @return 0 on the initial call, non-zero when returning via longjmp.
 */
int		setjmp(jmp_buf env) __attribute__((returns_twice));

/**
 * @brief Restore a saved execution context.
 *
 * @param env Buffer previously filled by setjmp.
 * @param val Value to return from setjmp (0 becomes 1).
 */
__HAJ_NORETURN
void	longjmp(jmp_buf env, int val);

/* ----- POSIX variants without signal mask ----- */

/**
 * @brief setjmp without saving the signal mask.
 *
 * On most systems, this is an alias for setjmp.
 */
int	_setjmp(jmp_buf env) __attribute__((returns_twice));

/**
 * @brief longjmp without restoring the signal mask.
 */
__HAJ_NORETURN
void	_longjmp(jmp_buf env, int val);

/* ----- POSIX variants with signal mask ----- */
/**
 * @brief Internal implementation of sigsetjmp.
 *
 * Defined in assembly. Do not call directly; use the sigsetjmp
 * macro below.
 */
int	__haj_sigsetjmp(sigjmp_buf env, int savesigs)
		__attribute__((returns_twice));

/**
 * @brief Save the current signal mask into env.
 *
 * Called by __haj_sigsetjmp (assembly) when savesigs != 0.
 *
 * @param env The jmp_buf to save the mask into.
 */
void	__haj_sigsetjmp_save(struct __haj_jmp_buf *env);

/**
 * @brief Save the context and optionally the signal mask.
 *
 * @param env      Buffer to save the context into.
 * @param savesigs If non-zero, the current signal mask is saved.
 * @return 0 on the initial call, non-zero when returning via
 *         siglongjmp.
 */
# define sigsetjmp(env, savesigs)	__haj_sigsetjmp(env, savesigs)

/**
 * @brief Restore a context saved by sigsetjmp.
 *
 * If the signal mask was saved, it is restored before the jump.
 *
 * @param env Buffer previously filled by sigsetjmp.
 * @param val Value to return from sigsetjmp (0 becomes 1).
 */
__HAJ_NORETURN
void	siglongjmp(sigjmp_buf env, int val);

# if defined(__cplusplus)
}
# endif

#endif /* _SETJMP_H */
