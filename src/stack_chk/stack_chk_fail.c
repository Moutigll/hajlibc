/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file stack_chk_fail.c
 * @brief Implementation of __stack_chk_fail.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/26 05:06:41 by Moutig
 *
 * WHEN IS THIS FUNCTION CALLED ?
 *
 * When the compiler detects that a stack canary has been
 * modified (indicating a buffer overflow), it calls
 * __stack_chk_fail().
 *
 * This function must:
 *   1. Print a diagnostic message to stderr.
 *   2. Terminate the process immediately.
 *
 * It must NOT return, because the program's state is corrupted.
 *
 * OUTPUT FORMAT
 *
 * The exact format is not specified, but glibc prints:
 *
 *   *** stack smashing detected ***: terminated
 *
 * We use a similar message.
 *
 * WHY NOT CALL abort() ?
 *
 * We could call abort(), which raises SIGABRT. But if the stack
 * is corrupted, calling abort() might itself crash. To be safe,
 * we do a direct syscall to exit_group, which terminates the
 * process without returning to user code.
 *
 * VERSIONS
 *
 * Some architectures use __stack_chk_fail_local instead of
 * __stack_chk_fail. This is a local variant that avoids the
 * PLT (Procedure Linkage Table) in position-independent
 * executables. We provide both for compatibility.
 */

#include <bits/os.h>
#include <bits/syscall.h>
#include <bits/compiler.h>

/* Forward declaration to avoid warnings */
__HAJ_NORETURN void	__stack_chk_fail(void);
__HAJ_NORETURN void	__stack_chk_fail_local(void);

/** --------------------------------------------------------------------------
 * Internal output helpers @TODO: Use fprintf() or write() instead of raw syscalls.
 * --------------------------------------------------------------------------
 *
 * We do not use printf() or write() because they might not be
 * safe to call in a corrupted stack state. We use raw syscalls.
 */

/**
 * @brief Write a string to stderr using a raw syscall.
 *
 * @param s NUL-terminated string.
 */
static void	__haj_stack_chk_write(const char *s)
{
	unsigned long	len = 0;

	while (s[len] != '\0')
		len++;
	if (len > 0)
		__haj_syscall3(SYS_write, 2, (long)s, (long)len);
}

/**
 * @brief Called by the compiler when a stack canary is corrupted.
 *
 * This function never returns. It prints a diagnostic message
 * and terminates the process.
 *
 * It is declared _Noreturn so that the compiler does not emit
 * code after the call.
 */
__HAJ_NORETURN void	__stack_chk_fail(void)
{
	__haj_stack_chk_write("*** stack smashing detected ***: terminated\n");

	/*
	 * Terminate the process.
	 *
	 * We use exit_group with status 134 (128 + SIGABRT). This
	 * matches what abort() would do. If the process has a
	 * handler for SIGABRT, it will not be called; we exit
	 * immediately.
	 */
	__haj_syscall1(SYS_exit_group, 134);

	/*
	 * Safety net. The syscall should never return.
	 */
	for (;;) {
	}
}

/*
 * Local variant used on some architectures to avoid PLT
 * indirection. It just calls __stack_chk_fail.
 */
__HAJ_NORETURN void	__stack_chk_fail_local(void)
{
	__stack_chk_fail();
}
