/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file main.c
 * @brief Test program for hajlib.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 18:36:30 by Moutig
 *
 * TODO: description.
 */

#include <haj/version.h>

#include <bits/syscall.h>

/*
 * We do not have write() or printf() implemented yet, so we use
 * the raw syscall for output.
 */
long	__haj_syscall6(long nr, long a1, long a2, long a3,
                       long a4, long a5, long a6);

/* ----- Minimal output ----- */

static void	putstr(const char *s)
{
	unsigned long	len = 0;

	while (s[len])
		len++;
	if (len > 0)
		__haj_syscall6(SYS_write, 1, (long)s, (long)len, 0, 0, 0);
}

/* ----- Main ----- */

int	main(void)
{
	putstr("hajlib version: ");
	putstr(HAJ_VERSION_STRING);
	putstr("\n");

	putstr("Hello from hajlib!\n");

	return (0);
}
