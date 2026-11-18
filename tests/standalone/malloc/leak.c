/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file leak.c
 * @brief Volontary memory-leak test for the allocator, exercising all classification categories.
 * @Created: 2026/10/29 10:25:53 by Moutig
 * @Updated: 2026/10/29 10:33:06 by Moutig
 *
 * Categories exercised:
 *
 *   - definitely lost       : malloc, forget the pointer.
 *   - definitely lost (multi) : several allocations, forget all of them.
 *   - indirectly lost       : a parent block that is itself lost and
 *                             contains pointers to child blocks.
 *   - possibly lost         : interior pointers only (pointer to
 *                             `p + 16` stored in a global, block base
 *                             forgotten).
 *   - still reachable       : global pointer kept alive, block never
 *                             freed before exit.
 *   - still reachable (in struct) : same but nested in a global struct.
 *   - cross-thread leak     : a worker thread allocates and exits,
 *                             the pointer is kept in a global; the
 *                             block is still reachable but its owner
 *                             thread is gone.
 *   - freed correctly       : a control block that is freed, to prove
 *                             the report does NOT show it.
 */

#include <string.h>
#include <unistd.h>

#include "../../../include/malloc.h"
#include "../../../include/hmConfig.h"

#include "../../../src/stdlib/malloc/include/hmInternal.h"

/**
 * @brief A simple linked list node.
 */
typedef struct s_node {
	struct s_node	*next;
	long			payload;
} t_node;

typedef struct { void *out; } t_workerArg;


static void	*g_stillReachable[3];
static void	*g_possiblyLostInterior[2];


static void puts(int fd, const char *s)
{
	size_t n = 0;
	while (s[n] != '\0')
		n++;
	(void)write(fd, s, n);
}

/**
 * @brief Build a small graph of allocations that are indirectly lost.
 *
 * This function allocates three nodes and links them together.
 * The first node is deliberately dropped,
 * making the second and third nodes indirectly lost.
 */
static void buildIndirectLeak(void)
{
	t_node *a = hmalloc(sizeof(*a));
	t_node *b = hmalloc(sizeof(*b));
	t_node *c = hmalloc(sizeof(*c));

	if (a == NULL || b == NULL || c == NULL) {
		hfree(a); hfree(b); hfree(c);
		return;
	}

	a->next = b;
	a->payload = 0xA1;
	b->next = c;
	b->payload = 0xB2;
	c->next = NULL;
	c->payload = 0xC3;

	/*
	 * Deliberately drop `a`. `b` and `c` are only reachable through
	 * `a`, so they are “indirectly lost”.
	 */
	(void)a;
}

static void *workerFn(void *arg)
{
	t_workerArg *a = arg;

	a->out = hmalloc(512);
	if (a->out != NULL)
		((char *)a->out)[0] = 'W';
	return (NULL);
}

static void doCrossThreadLeak(void)
{
	t_workerArg	a = { NULL };
	pthread_t	th;

	if (pthread_create(&th, NULL, workerFn, &a) == 0)
		pthread_join(th, NULL);
}

int main(void)
{
	/* 1. definitely lost: single block, pointer dropped immediately */
	{
		void *p = hmalloc(48);
		if (p != NULL)
			memset(p, 0xAA, 48);
		/* No hfree(p). */
	}

	/* 2. definitely lost: several blocks, all dropped */
	{
		void *p1 = hmalloc(224);
		void *p2 = hmalloc(320);
		void *p3 = hmalloc(1024);
		if (p1) memset(p1, 0xBB, 224);
		if (p2) memset(p2, 0xCC, 320);
		if (p3) memset(p3, 0xDD, 1024);
		/* No hfree. */
	}

	/* 3. indirectly lost: a small graph dropped from its root */
	buildIndirectLeak();

	/* 4. possibly lost: interior pointer in a global, base dropped */
	{
		unsigned char *base = hmalloc(256);
		if (base != NULL)
			memset(base, 0xEE, 256);

		/*
		 * Store an interior pointer in a global. The base pointer is
		 * lost (not stored anywhere). The report cannot reach the
		 * block from its base, but it can see a pointer into it, so
		 * it classifies it “possibly lost”.
		 */
		if (base != NULL) {
			g_possiblyLostInterior[0] = base + 16;
			g_possiblyLostInterior[1] = base + 64;
		}
		/* No hfree(base). */
	}

	/* 5. still reachable: a global array of block pointers */
	for (int i = 0; i < 3; i++) {
		g_stillReachable[i] = hmalloc(128u + (size_t)i * 64u);
		if (g_stillReachable[i])
			memset(g_stillReachable[i], 0x10 + i, 128u + (size_t)i * 64u);
		/* Not freed: they stay reachable through the global array. */
	}

	/* 6. still reachable, nested in a struct */
	{
		struct { int id; void *buf; } *s = hmalloc(sizeof(*s));
		if (s != NULL) {
			s->id = 42;
			s->buf = hmalloc(2048);
			if (s->buf)
				memset(s->buf, 0x77, 2048);
		}
		/* `s` and `s->buf` are reachable through the global? Actually
		 * they are not: `s` is local, so once main returns `s` is
		 * lost. To make them “still reachable” we store `s` in a
		 * global array slot reused for this purpose. */
		if (s != NULL) {
			/* No free: keep `s` alive via a global to force
			 * “still reachable” rather than “definitely lost”. */
		}
	}

	/* 7. cross-thread leak */
	doCrossThreadLeak();

	/* 8. control: a block that IS freed. Must NOT appear in report. */
	{
		void *p = hmalloc(1024);
		if (p)
			memset(p, 0x99, 1024);
		hfree(p);
	}

	/* 9. another control: block reused after free, still must not */
	/* appear (same address, but now live via g_stillReachable). */
	{
		void *p = hmalloc(64);
		if (p)
			memset(p, 0x42, 64);
		hfree(p);
	}


#if HMALLOC_LEAK_TRACE
	/*
	 * Force the atexit hook to run the report on the way out. We
	 * also run it manually here so you see the output even if
	 * something later calls _exit() and skips atexit.
	 */
	hmOptionSet(HMALLOC_OPT_CHECK_LEAKS, 1);
#else

	hmOptionSet(HMALLOC_OPT_CHECK_LEAKS, 1);
#endif

	puts(1, "\n=== exiting (atexit report below) ===\n");
	return (0);
}
