/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file arena.c
 * @brief Arena allocator for debuginfo module.
 * @Created: 2026/10/28 09:12:17 by Moutig
 * @Updated: 2026/10/28 11:49:54 by Moutig
 *
 * One chunk = one mmap of DI_CHUNK_SIZE bytes (or larger if a
 * single allocation does not fit). A chunk is never split, never
 * freed individually. di_arena_release() munmaps every chunk.
 *
 * The only heap operation is "give me n bytes" and "give me a
 * duplicate of this C string". No free, no realloc, no per-block
 * header. This keeps the module usable from a crash handler
 * (mmap and munmap are async-signal-safe) and free of any
 * dependency on the process allocator.
 */

#include "./internal.h"

#include <sys/mman.h>
#include <string.h>

static void *di_map(size_t size)
{
	void *p;

	if (size == 0) return NULL;
	p = mmap(NULL, size, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	return ((p == MAP_FAILED) ? NULL : p);
}

static void di_unmap(void *p, size_t size)
{
	if (p) (void)munmap(p, size);
}

void *di_arena_alloc(di_object *d, size_t n)
{
	void	*r;
	size_t	want;

	if (!d || n == 0) return (NULL);

	/* Keep every allocation 8-byte aligned. */
	n = (n + 7u) & ~(size_t)7u;

	if (n > d->left) {
		/* Need a new chunk. */
		if (d->nchunks >= DI_MAX_CHUNKS) return (NULL);

		want = (n > DI_CHUNK_SIZE) ? n : DI_CHUNK_SIZE;

		void *c = di_map(want);
		if (!c) return (NULL);

		d->chunks[d->nchunks]		= c;
		d->chunkSizes[d->nchunks]	= want;
		d->nchunks++;

		d->cur	= c;
		d->left	= want;
	}

	r = d->cur;
	d->cur	+= n;
	d->left	-= n;
	return (r);
}

void di_arena_release(di_object *d)
{
	if (!d) return;
	for (unsigned i = 0; i < d->nchunks; i++)
		di_unmap(d->chunks[i], d->chunkSizes[i]);
	d->nchunks	= 0;
	d->cur		= NULL;
	d->left		= 0;
}

const char *di_arena_strdup(di_object *d, const char *s)
{
	size_t	n;
	char	*r;

	if (!s) return (NULL);
	n = strlen(s) + 1;
	r = di_arena_alloc(d, n);
	if (!r) return (s);	/* fall back to caller's memory */
	memcpy(r, s, n);
	return (r);
}

const char *di_path_join(di_object *d, const char *dir, const char *name)
{
	size_t	dl, nl;
	char	*r;

	if (!name) return (NULL);
	if (name[0] == '/' || !dir || dir[0] == '\0')
		return di_arena_strdup(d, name);

	dl	= strlen(dir);
	nl	= strlen(name);
	r	= di_arena_alloc(d, dl + nl + 2);
	if (!r) return (name);

	memcpy(r, dir, dl);
	r[dl] = '/';
	memcpy(r + dl + 1, name, nl + 1);
	return (r);
}
