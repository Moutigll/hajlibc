/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file stack.c
 * @brief Implementation of __haj_threadAllocStack() and __haj_threadFreeStack().
 * @Created: 2026/09/30 07:02:18 by Moutig
 * @Updated: 2026/10/01 09:21:22 by Moutig
 *
 * Thread stacks are allocated with mmap(2) and freed with
 * munmap(2). A guard page is placed at the bottom of the stack
 * (the lowest address), so that a stack overflow triggers a
 * SIGSEGV instead of silently corrupting other memory.
 *
 * Stack layout (addresses grow downward):
 *
 *     high address
 *     +------------------+
 *     |   (unmapped)     |
 *     +------------------+
 *     |                  |
 *     |   usable stack   |  <- sp starts here
 *     |                  |
 *     +------------------+
 *     |   guard page     |  <- PROT_NONE
 *     +------------------+
 *     |   (unmapped)     |
 *     +------------------+
 *     low address
 *
 * The returned pointer is the LOW address of the whole mapping
 * (guard page included). The caller computes the stack top as
 * `base + size + guardSize`.
 */

#include <stddef.h>
#include <sys/auxv.h>
#include <unistd.h>
#include <sys/mman.h>
#include <bits/tls.h>
#include <bits/types.h>
#include <bits/os.h>
#include <errno.h>

/* Round `n` up to the next multiple of `align` (align must be
 * a power of two). */
static size_t hajRoundUp(size_t n, size_t align)
{
	return ((n + align - 1) & ~(align - 1));
}

void *__haj_threadAllocStack(size_t size, size_t guardSize)
{
	size_t	page = (size_t)sysconf(_SC_PAGESIZE);
	size_t	total;
	void	*base;

	/* Normalize the requested sizes. */
	if (size == 0)
		return (NULL);
	if (guardSize == 0)
		guardSize = page;

	size = hajRoundUp(size, page);
	guardSize = hajRoundUp(guardSize, page);

	total = size + guardSize;

	base = mmap(NULL, total, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS
#ifdef MAP_STACK
			   | MAP_STACK
#endif
			   , -1, 0);
	if (base == MAP_FAILED)
		return (NULL);

	/*
	 * Make the guard page at the bottom of the stack
	 * inaccessible. A stack overflow will touch it and the
	 * kernel will deliver SIGSEGV.
	 */
	if (mprotect(base, guardSize, PROT_NONE) != 0) {
		/* Free the mapping before returning. */
		munmap(base, total);
		return (NULL);
	}

	return (base);
}

void __haj_threadFreeStack(void *base, size_t size, size_t guardSize)
{
	size_t	page = (size_t)sysconf(_SC_PAGESIZE);
	size_t	total;

	if (base == NULL)
		return;

	if (guardSize == 0)
		guardSize = page;

	size = hajRoundUp(size, page);
	guardSize = hajRoundUp(guardSize, page);
	total = size + guardSize;

	munmap(base, total);
}
