/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file getenv.c
 * @brief Get the value of an environment variable.
 * @Created: 2026/10/05 17:36:34 by Moutig
 * @Updated: 2026/10/05 17:39:39 by Moutig
 *
 * This function retrieves the value of the environment variable specified by name.
 * If the variable is not found, it returns NULL.
 */

#include <stdlib.h>
#include <bits/crt.h>

char *getenv(const char *name)
{
	size_t	len;
	char	**env;

	if (name == NULL || environ == NULL)
		return (NULL);

	len = 0;
	while (name[len] != '\0')
		len++;

	for (env = environ; *env != NULL; env++) {
		const char	*entry = *env;
		size_t		i;

		for (i = 0; i < len; i++) {
			if (entry[i] != name[i])
				break;
		}
		if (i != len)
			continue;
		if (entry[len] != '=')
			continue;
		return ((char *)(entry + len + 1));
	}
	return (NULL);
}
