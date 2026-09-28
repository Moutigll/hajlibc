/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file assert.c
 * @brief Implementation of assertion failure handlers.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/26 05:04:28 by Moutig
 *
 * When an assertion fails, the assert() macro calls
 * __assert_fail(). This function prints a diagnostic message to
 * stderr and terminates the process with abort().
 *
 * OUTPUT FORMAT
 *
 * The output format is not specified by the C standard, but
 * every libc uses roughly the same:
 *
 *   prog: file.c:42: func: Assertion `expr' failed.
 *
 * We use this format for compatibility with tools that parse
 * assertion messages (test runners, CI, etc.).
 */

#include <assert.h>

#include <bits/os.h>
#include <bits/syscall.h>
#include <bits/crt.h>

/** --------------------------------------------------------------------------
 * Minimal output helpers
 * --------------------------------------------------------------------------
 * @TODO: Use fprintf() or fprintf() instead of __haj_syscall3(SYS_write, ...).
 * We do not use printf() or fprintf() because they are not
 * implemented yet, and because __assert_fail must work even in
 * a minimal environment (before stdio is initialized).
 */

/**
 * @brief Write a string to stderr, without using the libc.
 *
 * @param s NUL-terminated string.
 */
static void	__haj_assert_write_str(const char *s)
{
	unsigned long	len = 0;

	while (s[len] != '\0')
		len++;
	if (len > 0)
		__haj_syscall3(SYS_write, 2, (long)s, (long)len);
}

/**
 * @brief Write an unsigned integer to stderr.
 *
 * @param n Value to write.
 */
static void	__haj_assert_write_uint(unsigned int n)
{
	char	buf[16];
	int	i = 0;

	if (n == 0) {
		__haj_syscall3(SYS_write, 2, (long)"0", 1);
		return;
	}
	while (n > 0) {
		buf[i++] = '0' + (n % 10);
		n /= 10;
	}
	while (i > 0) {
		i--;
		__haj_syscall3(SYS_write, 2, (long)&buf[i], 1);
	}
}

/**
 * @brief Write a signed integer to stderr.
 *
 * @param n Value to write.
 */
static void	__haj_assert_write_int(int n)
{
	unsigned int	u;

	if (n < 0) {
		__haj_syscall3(SYS_write, 2, (long)"-", 1);
		u = (unsigned int)(-(n + 1)) + 1;
	} else {
		u = (unsigned int)n;
	}
	__haj_assert_write_uint(u);
}

__HAJ_NORETURN
void	__assert_fail(const char	*expr,
					  const char	*file,
					  int			line,
					  const char	*func)
{
	__haj_assert_write_str(__progname);
	__haj_assert_write_str(": ");
	__haj_assert_write_str(file ? file : "(unknown)");
	__haj_assert_write_str(":");
	__haj_assert_write_int(line);
	__haj_assert_write_str(": ");
	__haj_assert_write_str(func ? func : "(unknown)");
	__haj_assert_write_str(": Assertion `");
	__haj_assert_write_str(expr ? expr : "(null)");
	__haj_assert_write_str("' failed.\n");

	/*
	 * Terminate the process.
	 *
	 * We call abort() which raises SIGABRT. If the program has
	 * a handler for SIGABRT, it can catch the assertion. If
	 * not, the process is killed.
	 *
	 * If abort() is not implemented, we do the syscall directly
	 * (SYS_exit_group with status 134 = 128 + SIGABRT).
	 */
	__haj_syscall1(SYS_exit_group, 134);
	for (;;) {
	}
}

__HAJ_NORETURN
void	__assert_perror_fail(int		errnum,
							 const char	*file,
							 int		line,
							 const char	*func)
{
	__haj_assert_write_str(__progname);
	__haj_assert_write_str(": ");
	__haj_assert_write_str(file ? file : "(unknown)");
	__haj_assert_write_str(":");
	__haj_assert_write_int(line);
	__haj_assert_write_str(": ");
	__haj_assert_write_str(func ? func : "(unknown)");
	__haj_assert_write_str(": Assertion failed with errno ");
	__haj_assert_write_int(errnum);
	__haj_assert_write_str(".\n");

	__haj_syscall1(SYS_exit_group, 134);
	for (;;) {
	}
}
