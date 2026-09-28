/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file errno.h
 * @brief Error reporting.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 16:14:56 by Moutig
 *
 * This header defines the errno macro and the error codes
 * (EPERM, ENOENT, ...). The errno value is thread-local on all
 * supported platforms.
 *
 * The error codes themselves are defined in <bits/errno.h>
 * with per-OS values.
 */

#ifndef _ERRNO_H
# define _ERRNO_H

# include <bits/errno.h>
# include <bits/compiler.h>

/*
 * errno
 * -----
 *
 * The errno macro expands to a thread-local lvalue of type int.
 * Functions set errno on failure; user code reads it.
 *
 * In a libc replacement, errno must be thread-local. We use the
 * compiler's TLS support (__thread on GCC/Clang, __declspec(thread)
 * on MSVC) to declare it.
 *
 * The actual storage is defined in src/errno/errno.c.
 */

# if defined(__cplusplus)
extern "C" {
# endif

/**
 * @brief The thread-local errno variable.
 *
 * This variable is set by functions on error. It is thread-local, so each thread has its own errno value.
 */
extern __HAJ_THREAD_LOCAL int	__haj_errno;

# if defined(__cplusplus)
}
# endif

# ifndef errno
#  define errno	__haj_errno
# endif

#endif /* _ERRNO_H */
