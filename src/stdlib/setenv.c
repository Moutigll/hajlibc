/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file setenv.c
 * @brief Set, unset, and put environment variables.
 * @Created: 2026/10/05 17:37:59 by Moutig
 * @Updated: 2026/10/05 17:46:17 by Moutig
 *
 *
 * setenv() copies the name and value into a fresh "NAME=value"
 * string and either replaces an existing entry in the global
 * environment array or appends a new one. The array itself is
 * reallocated when it grows.
 *
 * unsetenv() removes an entry by shifting the remaining
 * entries down.
 *
 * putenv() takes a caller-owned "NAME=value" string and does
 * NOT copy it. This is the BSD behaviour, kept for
 * compatibility. It is dangerous (the caller must keep the
 * string alive and unmodified) and setenv should be preferred.
 *
 * All functions are not thread-safe: POSIX says the behaviour
 * is undefined if two threads modify the environment
 * concurrently. In practice, most programs set up the
 * environment once at startup.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <bits/crt.h>
#include <bits/types.h>


static char	**g_envAllocated = NULL;
static size_t g_envCapacity = 0;

/* ----- Helpers ----- */

static size_t hajEnvCount(char **env)
{
	size_t	n = 0;

	if (env == NULL)
		return (0);
	while (env[n] != NULL)
		n++;
	return (n);
}

/*
 * Ensure that `environ` points to a heap-allocated array that
 * we own. Called before any modification.
 *
 * On success, returns 0 and sets g_envAllocated and
 * g_envCapacity. On failure, returns -1 with errno set.
 */
static int hajEnvTakeOwnership(void)
{
	size_t	count;
	size_t	capacity;
	size_t	i;
	char	**copy;

	if (g_envAllocated != NULL)
		return (0);

	count = hajEnvCount(environ);
	capacity = count + 16;	/* slack for future additions */

	copy = malloc(capacity * sizeof(char *));
	if (copy == NULL)
		return (-1);

	for (i = 0; i < count; i++)
		copy[i] = environ[i];
	copy[count] = NULL;

	/*
	 * Clear the remaining slots. Not strictly necessary
	 * (they are beyond the NULL terminator), but keeps the
	 * array in a defined state.
	 */
	for (i = count + 1; i < capacity; i++)
		copy[i] = NULL;

	g_envAllocated = copy;
	g_envCapacity = capacity;
	environ = copy;
	return (0);
}

/*
 * Ensure that the array has room for at least `needed` slots
 * (including the terminating NULL). Called before an append.
 */
static int hajEnvEnsureCapacity(size_t needed)
{
	size_t	newCapacity;
	char	**newArray;

	if (hajEnvTakeOwnership() != 0)
		return (-1);

	if (needed <= g_envCapacity)
		return (0);

	newCapacity = g_envCapacity * 2;
	if (newCapacity < needed)
		newCapacity = needed + 16;

	newArray = realloc(g_envAllocated, newCapacity * sizeof(char *));
	if (newArray == NULL)
		return (-1);

	/*
	 * Zero the new slots. realloc does not initialize them.
	 */
	for (size_t i = g_envCapacity; i < newCapacity; i++)
		newArray[i] = NULL;

	g_envAllocated = newArray;
	g_envCapacity = newCapacity;
	environ = newArray;
	return (0);
}


/* ----- setenv ----- */

int setenv(const char *name, const char *value, int overwrite)
{
	size_t	nameLen;
	size_t	valueLen;
	size_t	totalLen;
	char	*entry;
	char	**env;
	size_t	index;

	if (name == NULL || value == NULL) {
		errno = EINVAL;
		return (-1);
	}
	/*
	 * POSIX: the name must not contain '='. We reject it.
	 */
	if (strchr(name, '=') != NULL) {
		errno = EINVAL;
		return (-1);
	}
	if (name[0] == '\0') {
		errno = EINVAL;
		return (-1);
	}

	nameLen = strlen(name);
	valueLen = strlen(value);

	/*
	 * Look for an existing entry.
	 */
	index = (size_t)-1;
	for (env = environ; env != NULL && *env != NULL; env++) {
		const char	*e = *env;
		size_t		i;

		for (i = 0; i < nameLen; i++) {
			if (e[i] != name[i])
				break;
		}
		if (i != nameLen)
			continue;
		if (e[nameLen] != '=')
			continue;
		index = (size_t)(env - environ);
		break;
	}

	if (index != (size_t)-1 && !overwrite)
		return (0);	/* already set, no change */

	totalLen = nameLen + 1 + valueLen + 1;
	entry = malloc(totalLen);
	if (entry == NULL)
		return (-1);

	memcpy(entry, name, nameLen);
	entry[nameLen] = '=';
	memcpy(entry + nameLen + 1, value, valueLen);
	entry[totalLen - 1] = '\0';

	if (index != (size_t)-1) {
		/*
		 * Replace: free the old entry and store the new
		 * one. The old entry might be a string that the
		 * caller passed to putenv, but we cannot know, so
		 * we do not free it. This is a known limitation
		 * of the POSIX model: setenv and putenv can
		 * coexist, but the ownership rules are messy.
		 */
		environ[index] = entry;
		return (0);
	}

	/*
	 * Append: grow the array if needed, then write at the
	 * current end and add the NULL terminator.
	 */
	{
		size_t	count = hajEnvCount(environ);

		if (hajEnvEnsureCapacity(count + 2) != 0) {
			free(entry);
			return (-1);
		}
		environ[count] = entry;
		environ[count + 1] = NULL;
	}
	return (0);
}


/* ----- unsetenv ----- */

int unsetenv(const char *name)
{
	size_t	nameLen;
	size_t	i;

	if (name == NULL || name[0] == '\0') {
		errno = EINVAL;
		return (-1);
	}
	if (strchr(name, '=') != NULL) {
		errno = EINVAL;
		return (-1);
	}

	nameLen = strlen(name);

	for (i = 0; environ != NULL && environ[i] != NULL; i++) {
		const char	*e = environ[i];
		size_t		j;

		for (j = 0; j < nameLen; j++) {
			if (e[j] != name[j])
				break;
		}
		if (j != nameLen)
			continue;
		if (e[nameLen] != '=')
			continue;

		/*
		 * Found. Shift the remaining entries down by one
		 * and add a NULL terminator. The removed entry
		 * leaks if it was allocated by setenv; POSIX
		 * does not require us to free it.
		 */
		for (; environ[i] != NULL; i++)
			environ[i] = environ[i + 1];
		return (0);
	}
	return (0);	/* not found: POSIX says this is not an error */
}


/* ----- putenv ----- */

/*
 * The BSD putenv takes a caller-owned "NAME=value" string and
 * stores the pointer directly in the environment. The string
 * must remain valid for the lifetime of the process.
 *
 * We find the '=' and check if an entry with the same name
 * already exists. If so, we replace the pointer; otherwise
 * we append.
 */
int putenv(char *string)
{
	char	*eq;
	size_t	nameLen;
	size_t	index;
	size_t	i;

	if (string == NULL) {
		errno = EINVAL;
		return (-1);
	}

	eq = strchr(string, '=');
	if (eq == NULL)
		return (unsetenv(string));

	nameLen = (size_t)(eq - string);
	if (nameLen == 0) {
		errno = EINVAL;
		return (-1);
	}

	index = (size_t)-1;
	for (i = 0; environ != NULL && environ[i] != NULL; i++) {
		const char	*e = environ[i];
		size_t		j;

		for (j = 0; j < nameLen; j++) {
			if (e[j] != string[j])
				break;
		}
		if (j != nameLen)
			continue;
		if (e[nameLen] != '=')
			continue;
		index = i;
		break;
	}

	if (index != (size_t)-1) {
		environ[index] = string;
		return (0);
	}

	{
		size_t	count = hajEnvCount(environ);

		if (hajEnvEnsureCapacity(count + 2) != 0)
			return (-1);
		environ[count] = string;
		environ[count + 1] = NULL;
	}
	return (0);
}
