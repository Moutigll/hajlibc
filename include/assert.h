/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file assert.h
 * @brief Assertions.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 16:14:25 by Moutig
 *
 * This header defines the assert() macro and the __assert_fail
 * function. When NDEBUG is defined, assert() becomes a no-op.
 */

#ifndef _ASSERT_H
# define _ASSERT_H

# include <bits/os.h>
# include <bits/arch.h>
# include <bits/compiler.h>

# if defined(__cplusplus)
extern "C" {
# endif

/* ----- Internal function called by the assert() macro on failure. ----- */
/**
 * The C standard requires the assert macro to print a diagnostic
 * containing the failing expression, the file name, the line
 * number, and the enclosing function name, then call abort().
 *
 * We use a helper function instead of inlining the code in the
 * macro to save space: the same function is called from every
 * assertion site.
 *
 * The function never returns.
 */

/**
 * @brief Called by the assert() macro when an assertion fails.
 *
 * @param expr  The expression that failed (as a string).
 * @param file  The source file name (__FILE__).
 * @param line  The line number (__LINE__).
 * @param func  The function name (__func__).
 */
__HAJ_NORETURN
void	__assert_fail(const char	*expr,
					  const char	*file,
					  int			line,
					  const char	*func);

/**
 * @brief Called by the assert() macro when an assertion fails.
 *
 * @param errnum  The error number (errno).
 * @param file    The source file name (__FILE__).
 * @param line    The line number (__LINE__).
 * @param func    The function name (__func__).
 */
__HAJ_NORETURN
void	__assert_perror_fail(int		errnum,
							 const char	*file,
							 int		line,
							 const char	*func);

/**
 * @brief The assert() macro.
 *
 * If the expression is false, print a diagnostic and terminate
 * the process. If NDEBUG is defined, this macro does nothing.
 *
 * The diagnostic format is:
 *
 *   prog: file.c:42: func: Assertion `expr' failed.
 *
 * where prog is the program name, file.c is the source file,
 * 42 is the line number, func is the function name, and expr is
 * the expression that failed.
 *
 * The program name is taken from __progname, which is set by
 * crt0. If __progname is not available, it falls back to "(null)".
 */

# ifdef NDEBUG
#  define assert(expr)	((void)0)
# else
#  define assert(expr) \
	((expr) \
	 ? (void)0 \
	 : __assert_fail(#expr, __FILE__, __LINE__, __func__))
# endif

# if defined(__cplusplus)
}
# endif

#endif /* _ASSERT_H */
