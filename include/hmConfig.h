/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file hmConfig.h
 * @brief Single source of truth for build-time configuration of the memory allocator.
 * @Created: 2026/10/19 12:04:15 by Moutig
 * @Updated: 2026/10/19 12:28:36 by Moutig
 *
 * All macros can be overridden from the command line (`-D...`)
 * or from a Makefile target. Defaults aim for a production
 * build: no logging, no tracing, no profiling, no runtime
 * checks, stats collection on (cheap on the slow path).
 */

#ifndef HMALLOC_CONFIG_H
# define HMALLOC_CONFIG_H

/* ---------- Debug ---------- */

/*   0 = release
 *   1 = simple assertions
 *   2 = assertions + free-time pointer checks
 *   3 = + page integrity checks
 *   4 = + heap dump at init */
# ifndef HMALLOC_DEBUG
#  define HMALLOC_DEBUG	0
# endif

/**
 * @brief 8-byte canary in each block's padding.
 *
 * This canary is used to detect buffer overflows and other memory corruption issues.
 */
# ifndef HMALLOC_CANARY
#  define HMALLOC_CANARY	0
# endif

/**
 * @brief Runtime checks for memory errors.
 *
 * This enables runtime checks for invalid pointer dereferences, double frees, and incorrect sizes.
 */
# ifndef HMALLOC_CHECK
#  define HMALLOC_CHECK	0
# endif

/**
 * @brief XOR-obfuscated free-list pointers.
 *
 * The free-list is a singly-linked list of free blocks. Each
 * block's first word is a pointer to the next free block. This
 * obfuscation helps prevent certain types of memory corruption.
 */
# ifndef HMALLOC_ENCODE_FREELIST
#  define HMALLOC_ENCODE_FREELIST	0
# endif

/* ---------- Logging ---------- */

# ifndef HMALLOC_LOG_ENABLED
#  define HMALLOC_LOG_ENABLED	0
# endif

/* 0=silent 1=error 2=warn 3=info 4=debug 5=trace */
# ifndef HMALLOC_LOG_LEVEL
#  define HMALLOC_LOG_LEVEL	2
# endif

# ifndef HMALLOC_LOG_COLOR
#  define HMALLOC_LOG_COLOR	1
# endif

/* ---------- Trace ---------- */

/**
 * @brief Enable tracing of memory allocation and deallocation.
 *
 * When enabled, the library will log all allocation and deallocation
 * events to a trace buffer.
 */
# ifndef HMALLOC_TRACE
#  define HMALLOC_TRACE	0
# endif

# ifndef HMALLOC_TRACE_BUFSIZE
#  define HMALLOC_TRACE_BUFSIZE	16384
# endif

/* ---------- Profiling ---------- */

/**
 * @brief Enable profiling of memory usage.
 *
 * When enabled, the library will collect statistics on memory usage,
 * including allocation sizes and frequencies.
 */
# ifndef HMALLOC_PROFILE
#  define HMALLOC_PROFILE	0
# endif

# ifndef HMALLOC_PROFILE_RATE
#  define HMALLOC_PROFILE_RATE	(64 * 1024)
# endif

/* ---------- Stats & display ---------- */

/**
 * @brief Enable collection of memory usage statistics.
 *
 * When enabled, the library will collect and display statistics on memory usage.
 */
# ifndef HMALLOC_STATS
#  define HMALLOC_STATS	1
# endif

# ifndef HMALLOC_STATS_DISPLAY
#  define HMALLOC_STATS_DISPLAY	1
# endif

# ifndef HMALLOC_STATS_WIDTH
#  define HMALLOC_STATS_WIDTH	100
# endif

/* ---------- Sanitizers (auto-detected) ---------- */

# if defined(__SANITIZE_ADDRESS__)
#  define HMALLOC_HAS_ASAN	1
# else
#  define HMALLOC_HAS_ASAN	0
# endif

# if defined(__SANITIZE_THREAD__)
#  define HMALLOC_HAS_TSAN	1
# else
#  define HMALLOC_HAS_TSAN	0
# endif

#endif /* HMALLOC_CONFIG_H */
