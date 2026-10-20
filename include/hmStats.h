/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file hmStats.h
 * @brief Statistics collection and display for the memory allocator.
 * @Created: 2026/10/19 12:21:12 by Moutig
 * @Updated: 2026/10/19 16:02:02 by Moutig
 *
 * This file defines the data structures and functions for collecting
 * and displaying statistics about memory usage in the allocator.
 */

#ifndef HMALLOC_STATS_H
# define HMALLOC_STATS_H

# include <stddef.h>

# include "hmConfig.h"

/* ----- Bin geometry ----- */

/**
 * @brief Maximum size for a "tiny" allocation.
 *
 * Allocations larger than this size are considered "huge" and are
 * handled differently by the allocator.
 */
# define HMALLOC_BIN_HUGE		73
# define HMALLOC_BIN_FULL		(HMALLOC_BIN_HUGE + 1)
# define HMALLOC_BIN_COUNT		(HMALLOC_BIN_FULL + 1)

/**
 * @brief Maximum size for a "tiny" allocation.
 *
 * Allocations under this size follow a "fast-path" allocation strategy using a direct index table.
 * Allocations larger than this size are handled differently by the allocator.
 */
# ifndef HMALLOC_TINY_MAX_SIZE
#  define HMALLOC_TINY_MAX_SIZE	4096
# endif

/**
 * @brief Number of direct page allocations for tiny allocations.
 *
 * This is calculated based on the maximum size for tiny allocations.
 * It determines how many direct page allocations are available for tiny allocations.
 */
# define HMALLOC_PAGES_DIRECT	((HMALLOC_TINY_MAX_SIZE >> 3) + 2)

/* ----- Stats snapshot ----- */

# define HMALLOC_STATS_VERSION	1

/**
 * @brief Snapshot of the allocator statistics.
 *
 * The caller fills `size` and `version` before calling
 * hmStatsGet (hmStatsInit does this).
 *
 * When HMALLOC_STATS == 0, the collection code is not compiled
 * in, but the type is still available so the public API links.
 */
typedef struct s_hmallocStats {
	size_t	size;		/* = sizeof(t_hmallocStats) */
	int		version;	/* = HMALLOC_STATS_VERSION */

	size_t	total;		/* Total memory allocated by the allocator. */
	size_t	peak;		/* Peak memory usage recorded by the allocator. */
	size_t	current;	/* Current memory usage recorded by the allocator. */

	size_t	pages;		/* Total number of pages allocated by the allocator. */
	size_t	reserved;	/* Total reserved memory by the allocator. */
	size_t	committed;	/* Total committed memory by the allocator. */
	size_t	mmapCalls;	/* Total number of mmap calls made by the allocator. */
	size_t	arenaCount;	/* Total number of arenas created by the allocator. */

	size_t	mallocNormalCount;	/* Total number of normal malloc calls made by the allocator. */
	size_t	mallocHugeCount;	/* Total number of huge malloc calls made by the allocator. */
	size_t	mallocRequested;	/* Total number of bytes requested by malloc calls. */
	size_t	errors;				/* Total number of errors encountered by the allocator. */

	size_t	mallocBins[HMALLOC_BIN_COUNT];	/* Total number of allocations per bin size. */
	size_t	pageBins[HMALLOC_BIN_COUNT];	/* Total number of pages allocated per bin size. */
} t_hmallocStats;

/**
 * @brief Zero-initialise a stats snapshot and set the header.
 * @param s Structure to initialise.
 */
static inline void	hmStatsInit(t_hmallocStats *s)
{
	unsigned char	*p = (unsigned char *)s;

	for (size_t i = 0; i < sizeof(*s); i++)
		p[i] = 0;
	s->size = sizeof(*s);
	s->version = HMALLOC_STATS_VERSION;
}

#endif /* HMALLOC_STATS_H */
