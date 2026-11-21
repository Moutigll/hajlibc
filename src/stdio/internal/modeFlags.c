/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file modeFlags.c
 * @brief Translate fopen() mode strings to open(2) and FILE flags.
 * @Created: 2026/11/21 23:39:06 by Moutig
 * @Updated: 2026/11/21 23:41:52 by Moutig
 *
 * Parses the mode string accepted by fopen/freopen/fdopen and turns
 * it into the O_* flags for open(2) plus the initial F_NORD / F_NOWR
 * flags for the FILE.
 *
 * Accepted characters:
 *   'r'  open for reading (file must exist)
 *   'w'  open for writing, truncate (create if missing)
 *   'a'  open for writing, append (create if missing)
 *   '+'  read/write instead of read-only or write-only
 *   'b'  ignored (POSIX: no effect)
 *   'x'  O_EXCL, exclusive create (C11)
 *   'e'  O_CLOEXEC (POSIX.1-2024)
 *
 * The first character must be one of 'r', 'w' or 'a'. Any other
 * character that is not a valid modifier is rejected with EINVAL.
 * The function never allocates and never touches the filesystem, so
 * it is trivially testable and can be reused by freopen, fdopen and
 * the future popen.
 */

#include "stdioImpl.h"
#include <errno.h>
#include <fcntl.h>

/**
 * @brief Translate the first character of a mode string.
 *
 * Sets the base O_* flags and the initial permission flags:
 *   'r' -> O_RDONLY,                    F_NOWR
 *   'w' -> O_WRONLY | O_CREAT | O_TRUNC, F_NORD
 *   'a' -> O_WRONLY | O_CREAT | O_APPEND, F_NORD | F_APP
 *
 * @param c       First character of the mode string.
 * @param oflags  Output: O_* flags for open(2).
 * @param fdflags Output: initial F_NORD / F_NOWR flags.
 * @return 0 on success, -1 on invalid base character (errno = EINVAL).
 */
static int	hajBaseMode(int c, int *oflags, int *fdflags)
{
	if (c == 'r')
	{
		*oflags = O_RDONLY;
		*fdflags = F_NOWR;
		return (0);
	}
	if (c == 'w')
	{
		*oflags = O_WRONLY | O_CREAT | O_TRUNC;
		*fdflags = F_NORD;
		return (0);
	}
	if (c == 'a')
	{
		*oflags = O_WRONLY | O_CREAT | O_APPEND;
		*fdflags = F_NORD | F_APP;
		return (0);
	}
	errno = EINVAL;
	return (-1);
}

/**
 * @brief Apply one modifier character to the mode.
 *
 *   '+' -> switch to O_RDWR, clear F_NORD and F_NOWR, drop O_APPEND
 *   'b' -> ignored (POSIX: no effect)
 *   'x' -> add O_EXCL (C11 exclusive create)
 *   'e' -> add O_CLOEXEC (POSIX.1-2024)
 *
 * @param c       Modifier character.
 * @param oflags  In/out: O_* flags for open(2).
 * @param fdflags In/out: initial F_NORD / F_NOWR flags.
 * @return 0 on success, -1 on invalid modifier (errno = EINVAL).
 */
static int	hajModifier(int c, int *oflags, int *fdflags)
{
	if (c == '+')
	{
		*oflags = (*oflags & ~(O_RDONLY | O_WRONLY | O_APPEND))
			| O_RDWR;
		*fdflags &= ~(F_NORD | F_NOWR);
		return (0);
	}
	if (c == 'b')
		return (0);
	if (c == 'x')
	{
		*oflags |= O_EXCL;
		return (0);
	}
	if (c == 'e')
	{
		*oflags |= O_CLOEXEC;
		return (0);
	}
	errno = EINVAL;
	return (-1);
}

int	__haj_modeToFlags(const char *mode, int *oflags, int *fdflags)
{
	int	i;

	if (mode == NULL || oflags == NULL || fdflags == NULL)
	{
		errno = EINVAL;
		return (-1);
	}
	if (hajBaseMode((unsigned char)mode[0], oflags, fdflags) < 0)
		return (-1);
	i = 1;
	while (mode[i] != '\0')
	{
		if (hajModifier((unsigned char)mode[i], oflags, fdflags) < 0)
			return (-1);
		i++;
	}
	return (0);
}
