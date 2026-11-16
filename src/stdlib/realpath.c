/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file realpath.c
 * @brief POSIX realpath implementation.
 * @Created: 2026/11/16 14:53:38 by Moutig
 * @Updated: 2026/11/16 14:53:53 by Moutig
 *
 * This file implements the realpath() function for Linux systems. It resolves
 * the absolute path of a given file or directory, handling symbolic links,
 * relative paths, and special components like "." and "..". The function
 * returns a pointer to the resolved absolute path, or NULL on error, setting
 * errno accordingly. The implementation ensures that the resolved path is
 * always null-terminated and does not exceed PATH_MAX.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <stdbool.h>

/* Same limit as Linux / POSIX SYMLOOP_MAX. */
#define HAJ_REALPATH_SYMLOOP	40

/* Check that the path in `out` (length n) names a directory. */
static int	realpathCheckDir(char *out, size_t n)
{
	char	dummy;

	if (n + 2 >= PATH_MAX)
		return (ENAMETOOLONG);
	out[n] = (n == 1) ? '.' : '/';
	out[n + 1] = (n == 1) ? '\0' : '.';
	out[n + 2] = '\0';
	if (readlink(out, &dummy, 1) >= 0 || errno != EINVAL) {
		int err = (errno == EINVAL) ? ENOTDIR : errno;
		out[n] = '\0';
		return (err);
	}
	out[n] = '\0';
	return (0);
}

char	*realpath(const char *restrict path, char *restrict resolved)
{
	char	todo[PATH_MAX];		/* components still to resolve */
	char	out[PATH_MAX];		/* canonical prefix resolved so far */
	char	link[PATH_MAX];
	size_t	pos;				/* read position in todo */
	size_t	tlen;				/* length of todo */
	size_t	n;					/* length of out */
	int		links = 0;
	int		err = 0;
	int		saved = errno;
	bool	needDir = false;	/* last component not yet known to be a dir */

	if (path == NULL) {
		errno = EINVAL;
		return (NULL);
	}
	tlen = strnlen(path, PATH_MAX);
	if (tlen == 0) {
		errno = ENOENT;
		return (NULL);
	}
	if (tlen >= PATH_MAX) {
		errno = ENAMETOOLONG;
		return (NULL);
	}
	memcpy(todo, path, tlen + 1);

	if (todo[0] == '/') {
		out[0] = '/';
		out[1] = '\0';
		n = 1;
	} else {
		if (getcwd(out, sizeof(out)) == NULL)
			return (NULL);
		n = strlen(out);
	}

	pos = 0;
	while (err == 0) {
		size_t	start;
		size_t	clen;

		while (todo[pos] == '/')
			pos++;
		if (todo[pos] == '\0')
			break;
		start = pos;
		while (todo[pos] != '/' && todo[pos] != '\0')
			pos++;
		clen = pos - start;

		/* "." and ".." need the current prefix to be a directory. */
		if ((clen == 1 && todo[start] == '.')
			|| (clen == 2 && todo[start] == '.' && todo[start + 1] == '.')) {
			if (needDir && (err = realpathCheckDir(out, n)) != 0)
				break;
			needDir = false;
			if (clen == 2) {
				while (n > 1 && out[n - 1] != '/')
					n--;
				if (n > 1)
					n--;
				out[n] = '\0';
			}
			continue;
		}

		/* Append "/<component>". */
		{
			size_t	prev = n;
			ssize_t	r;

			if (n + (n > 1) + clen >= PATH_MAX) {
				err = ENAMETOOLONG;
				break;
			}
			if (n > 1)
				out[n++] = '/';
			memcpy(out + n, todo + start, clen);
			n += clen;
			out[n] = '\0';

			r = readlink(out, link, sizeof(link));
			if (r < 0) {
				if (errno != EINVAL) {
					err = errno;
					break;
				}
				needDir = true;		/* exists, not a symlink */
				continue;
			}
			if ((size_t)r >= sizeof(link)) {
				err = ENAMETOOLONG;
				break;
			}
			if (++links > HAJ_REALPATH_SYMLOOP) {
				err = ELOOP;
				break;
			}
			if (r == 0) {
				err = ENOENT;
				break;
			}

			/* Splice: todo = link + rest (rest is empty or starts with '/'). */
			{
				size_t	rest = tlen - pos;

				if ((size_t)r + rest >= PATH_MAX) {
					err = ENAMETOOLONG;
					break;
				}
				memmove(todo + r, todo + pos, rest + 1);
				memcpy(todo, link, (size_t)r);
				tlen = (size_t)r + rest;
				pos = 0;
			}

			/* Drop the link itself; an absolute target restarts at "/". */
			n = (link[0] == '/') ? 1 : prev;
			out[n] = '\0';
			needDir = false;
		}
	}

	/* A trailing slash requires a directory. */
	if (err == 0 && needDir && tlen > 0 && todo[tlen - 1] == '/')
		err = realpathCheckDir(out, n);
	if (err != 0) {
		errno = err;
		return (NULL);
	}

	errno = saved;
	if (resolved == NULL) {
		resolved = (char *)malloc(n + 1);
		if (resolved == NULL) {
			errno = ENOMEM;
			return (NULL);
		}
	}
	memcpy(resolved, out, n + 1);
	return (resolved);
}
