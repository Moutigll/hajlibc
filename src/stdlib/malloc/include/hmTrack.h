/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file hmTrack.h
 * @brief Memory tracking hooks (valgrind, ASan).
 * @Created: 2026/10/05 17:14:20 by Moutig
 * @Updated: 2026/10/09 20:40:59 by Moutig
 *
 * The allocator can inform external tools about the state of
 * each block: valgrind's memcheck, AddressSanitizer, or
 * nothing at all. The macros below are no-ops when no tool is
 * enabled, so they can be called unconditionally.
 */

#ifndef HMALLOC_TRACK_H
# define HMALLOC_TRACK_H

# if defined(HMALLOC_TRACK_VALGRIND)

#  define HMALLOC_TRACK_ENABLED	1
#  include <valgrind/valgrind.h>
#  include <valgrind/memcheck.h>

#  define hmTrackMalloc(p, req, size, zero)	VALGRIND_MALLOCLIKE_BLOCK(p, size, 0, zero)
#  define hmTrackFree(p, size)				VALGRIND_FREELIKE_BLOCK(p, 0)
#  define hmTrackMemDefined(p, size)		VALGRIND_MAKE_MEM_DEFINED(p, size)
#  define hmTrackMemUndefined(p, size)		VALGRIND_MAKE_MEM_UNDEFINED(p, size)
#  define hmTrackMemNoaccess(p, size)		VALGRIND_MAKE_MEM_NOACCESS(p, size)

# elif defined(HMALLOC_TRACK_ASAN)

#  define HMALLOC_TRACK_ENABLED	1
#  include <sanitizer/asan_interface.h>

#  define hmTrackMalloc(p, req, size, zero)	ASAN_UNPOISON_MEMORY_REGION(p, size)
#  define hmTrackFree(p, size)				ASAN_POISON_MEMORY_REGION(p, size)
#  define hmTrackMemDefined(p, size)		ASAN_UNPOISON_MEMORY_REGION(p, size)
#  define hmTrackMemUndefined(p, size)		ASAN_UNPOISON_MEMORY_REGION(p, size)
#  define hmTrackMemNoaccess(p, size)		ASAN_POISON_MEMORY_REGION(p, size)

# else

#  define HMALLOC_TRACK_ENABLED	0

#  define hmTrackMalloc(p, req, size, zero)	do { (void)(p); (void)(req); (void)(size); (void)(zero); } while (0)
#  define hmTrackFree(p, size)				do { (void)(p); (void)(size); } while (0)
#  define hmTrackMemDefined(p, size)		do { (void)(p); (void)(size); } while (0)
#  define hmTrackMemUndefined(p, size)		do { (void)(p); (void)(size); } while (0)
#  define hmTrackMemNoaccess(p, size)		do { (void)(p); (void)(size); } while (0)

# endif

#endif /* HMALLOC_TRACK_H */
