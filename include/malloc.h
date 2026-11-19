/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file malloc.h
 * @brief Public API of the hmalloc allocator.
 * @Created: 2026/10/05 15:16:12 by Moutig
 * @Updated: 2026/11/19 11:56:47 by Moutig
 *
 * hmalloc is the dynamic memory allocator of hajlibc. It implements the
 * standard allocation functions (malloc, free, posix_memalign, operator
 * new, ...) and this extended API.
 *
 * It is built either into the standalone static libc, or as a shared
 * library loaded with LD_PRELOAD on top of glibc, where it replaces every
 * allocation entry point of the host libc and of the C++ runtime.
 *
 * The allocator is built on top of `mmap`/`munmap` only: it never calls
 * the host `malloc`, so it can be used to implement it (LD_PRELOAD).
 * Its fast path is lock-free and syscall-free.
 *
 * Contents
 * --------
 *   1. Standard allocation      hmalloc hcalloc hrealloc hfree hfreeSize
 *   2. Extended allocation      hzalloc hmallocn hmallocAligned
 *   3. Block sizes              husableSize hgoodSize hexpand
 *   4. Platform extensions      malloc_usable_size malloc_size
 *                               malloc_good_size cfree vfree _aligned_malloc
 *   5. Leak tracking            hmLeak* hmReportLeaks
 *   6. Statistics types         t_hmallocStats, bin geometry, chunk bins
 *   7. Heaps                    hmHeap* hmHeapOf hmHeapVisitBlocks
 *                               hmSubprocMain hmTheapGetDefault
 *   8. Global helpers           hmCollect hmIsRedirected hmProcessInfo
 *   9. Callbacks                hmRegister*
 *  10. 42 project               show_alloc_mem show_alloc_mem_ex
 *                               show_alloc_history
 *  11. Options                  hmOptionSet hmOptionGet
 *  12. Statistics               hmStats* hmSubprocStats* hmTheapStats*
 *                               (stubs when HMALLOC_STATS == 0)
 *  13. Profiling                hmProfile* (stubs when HMALLOC_PROFILE == 0)
 *
 * Statistics and profiling are compile-time optional (see <hmConfig.h>):
 * the API always links, but when HMALLOC_STATS / HMALLOC_PROFILE are 0
 * the corresponding calls are stubs returning false / doing nothing.
 */

#ifndef HMALLOC_H
# define HMALLOC_H

# include <stddef.h>
# include <stdint.h>
# include <stdbool.h>

# include "hmConfig.h"

# ifdef __cplusplus
extern "C" {
# endif

/* ----- Visibility and attributes ----- */

/*
 * The macros below select the correct annotation for the
 * current compiler. They are shared with the rest of hajlibc:
 * `_hmDeclExport` marks a symbol as part of the public ABI
 * when building a shared library, and the `_hmAttr*` macros
 * describe the semantics of the allocation functions to the
 * compiler (allocation size, alignment, restartable, etc.).
 *
 * They are defined here because the allocator is also
 * distributed as a standalone .so for the 42 project, outside
 * of the hajlibc build. In the hajlibc tree they are already
 * provided by <bits/compiler.h>.
 */

# if !defined(_hmDeclExport)

#  if defined(__cplusplus) && (__cplusplus >= 201103L)
#   define _hmAttrNoexcept		noexcept
#  elif defined(__cplusplus)
#   define _hmAttrNoexcept		throw()
#  else
#   define _hmAttrNoexcept
#  endif

#  if defined(__cplusplus) && (__cplusplus >= 201703L)
#   define _hmDeclNodiscard		[[nodiscard]]
#  elif defined(__GNUC__) && (__GNUC__ >= 4) || defined(__clang__)
#   define _hmDeclNodiscard		__attribute__((warn_unused_result))
#  elif defined(_HAS_NODISCARD)
#   define _hmDeclNodiscard		_NODISCARD
#  elif defined(_MSC_VER) && (_MSC_VER >= 1700)
#   define _hmDeclNodiscard		_Check_return_
#  else
#   define _hmDeclNodiscard
#  endif

#  if defined(_MSC_VER) || defined(__MINGW32__)

#   if !defined(HMALLOC_SHARED_LIB)
#    define _hmDeclExport
#   elif defined(HMALLOC_SHARED_LIB_EXPORT)
#    define _hmDeclExport		__declspec(dllexport)
#   else
#    define _hmDeclExport		__declspec(dllimport)
#   endif

#   if defined(__MINGW32__)
#    define _hmDeclRestrict
#    define _hmAttrMalloc		__attribute__((malloc))
#   else
#    if (_MSC_VER >= 1900) && !defined(__EDG__)
#     define _hmDeclRestrict	__declspec(allocator) __declspec(restrict)
#    else
#     define _hmDeclRestrict	__declspec(restrict)
#    endif
#    define _hmAttrMalloc
#   endif

#   define _hmCdecl			__cdecl
#   define _hmAttrAllocSize(s)
#   define _hmAttrAllocSize2(s1,s2)
#   define _hmAttrAllocAlign(p)

#  elif defined(__GNUC__) || defined(__clang__)

#   if defined(HMALLOC_SHARED_LIB) && defined(HMALLOC_SHARED_LIB_EXPORT)
#    define _hmDeclExport		__attribute__((visibility("default")))
#   else
#    define _hmDeclExport
#   endif

#   define _hmCdecl
#   define _hmDeclRestrict
#   define _hmAttrMalloc			__attribute__((malloc))
#   define _hmAttrAllocSize(s)		__attribute__((alloc_size(s)))
#   define _hmAttrAllocSize2(s1,s2)	__attribute__((alloc_size(s1,s2)))
#   define _hmAttrAllocAlign(p)		__attribute__((alloc_align(p)))

#  else

#   define _hmCdecl
#   define _hmDeclExport
#   define _hmDeclRestrict
#   define _hmAttrMalloc
#   define _hmAttrAllocSize(s)
#   define _hmAttrAllocSize2(s1,s2)
#   define _hmAttrAllocAlign(p)

#  endif

# endif /* !_hmDeclExport */

/* ----- Core API ----- */

/**
 * @brief Allocate an uninitialised block of memory.
 *
 * Allocates `size` bytes. The returned pointer is aligned for
 * any object whose size is at most the platform's maximum
 * fundamental alignment (16 bytes on x86_64 and aarch64).
 *
 * A request of size 0 returns a unique, non-NULL pointer that
 * can be passed to `hfree`. The content is undefined.
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to the allocated block, or NULL on failure
 *         (errno = ENOMEM).
 */
_hmDeclNodiscard _hmDeclExport _hmDeclRestrict
void	*hmalloc(size_t size)
		_hmAttrNoexcept _hmAttrMalloc _hmAttrAllocSize(1);

/**
 * @brief Allocate and zero-initialise an array.
 *
 * Allocates `count * size` bytes. If the multiplication
 * overflows, the request fails with errno = ENOMEM.
 *
 * @param count Number of elements.
 * @param size  Size of each element in bytes.
 * @return Pointer to the zeroed block, or NULL on failure.
 */
_hmDeclNodiscard _hmDeclExport _hmDeclRestrict
void	*hcalloc(size_t count, size_t size)
		_hmAttrNoexcept _hmAttrMalloc _hmAttrAllocSize2(1,2);

/**
 * @brief Resize a previously allocated block.
 *
 * Matches POSIX `realloc` semantics:
 *   - p == NULL      → hmalloc(newsize)
 *   - newsize == 0   → free(p) and return hmalloc(0)
 *   - failure        → p untouched, NULL returned
 *
 * @param p       Previously allocated block, or NULL.
 * @param newsize New size in bytes.
 * @return Pointer to the resized block, or NULL on failure.
 */
_hmDeclNodiscard _hmDeclExport
void	*hrealloc(void *p, size_t newsize)
		_hmAttrNoexcept _hmAttrAllocSize(2);

/**
 * @brief Release a block of memory.
 *
 * @param p Block to free, or NULL. Passing NULL is a no-op.
 */
_hmDeclExport
void	hfree(void *p)
		_hmAttrNoexcept;

/**
 * @brief Free a block whose size is known.
 *
 * Equivalent to `hfree(p)` but allows runtime size checks
 * when HMALLOC_CHECK is enabled.
 *
 * @param p    Block to free, or NULL.
 * @param size Size originally requested.
 */
_hmDeclExport
void	hfreeSize(void *p, size_t size)
		_hmAttrNoexcept;

/* ----- 2. Extended allocation ----- */

/**
 * @brief Allocate and zero-initialise a block.
 * Equivalent to `hcalloc(1, size)`.
 *
 * @param size Size in bytes.
 * @return Pointer to the zeroed block, or NULL on failure.
 */
_hmDeclNodiscard _hmDeclExport _hmDeclRestrict
void	*hzalloc(size_t size)
		_hmAttrNoexcept _hmAttrMalloc _hmAttrAllocSize(1);

/**
 * @brief Allocate an array without zero-initialising it.
 *
 * Allocates `count * size` bytes. Overflow is reported as
 * errno = ENOMEM.
 *
 * @param count Number of elements.
 * @param size  Size of each element in bytes.
 * @return Pointer to the block, or NULL on failure.
 */
_hmDeclNodiscard _hmDeclExport _hmDeclRestrict
void	*hmallocn(size_t count, size_t size)
		_hmAttrNoexcept _hmAttrMalloc _hmAttrAllocSize2(1,2);

/**
 * @brief Allocate a block with a specific alignment.
 *
 * The alignment must be a power of two. Values below 16 are
 * rounded up to the platform's natural alignment. The block
 * must be released with `hfree`.
 *
 * @param size      Size in bytes.
 * @param alignment Required alignment (power of two).
 * @return Pointer to the aligned block, or NULL on failure
 *         (errno = EINVAL or ENOMEM).
 */
_hmDeclNodiscard _hmDeclExport _hmDeclRestrict
void	*hmallocAligned(size_t size, size_t alignment)
		_hmAttrNoexcept _hmAttrMalloc
		_hmAttrAllocSize(1) _hmAttrAllocAlign(2);

/* ----- 3. Block sizes ----- */

/**
 * @brief Return the usable size of a block.
 *
 * @param p Previously allocated block, or NULL.
 * @return Usable size in bytes, or 0 if p is NULL.
 */
_hmDeclNodiscard _hmDeclExport
size_t	husableSize(const void *p)
		_hmAttrNoexcept;

/**
 * @brief Round a size up to the next allocatable bucket.
 *
 * @param size Requested size in bytes.
 * @return The usable size that `husableSize` would report
 *         for this request.
 */
_hmDeclNodiscard _hmDeclExport
size_t	hgoodSize(size_t size)
		_hmAttrNoexcept;

/**
 * @brief Try to enlarge a block in place.
 *
 * @param p       Previously allocated block.
 * @param newsize Requested new size in bytes.
 * @return p on success, NULL if the block cannot grow.
 */
_hmDeclExport
void	*hexpand(void *p, size_t newsize)
		_hmAttrNoexcept;

/* ----- 4. Platform extensions ----- */

/*
 * Non-standard names provided for compatibility with other platforms.
 * The standard and POSIX ones (malloc ... free_aligned_sized,
 * posix_memalign, memalign, valloc, pvalloc, reallocarray, reallocf,
 * reallocarr, strdup, strndup, realpath) are declared in <stdlib.h> and
 * <string.h>, under their usual names only.
 *
 * In the LD_PRELOAD build the allocator also exports the glibc internal
 * entry points (__libc_malloc, __libc_calloc, __libc_realloc, __libc_free,
 * __libc_cfree, __libc_memalign, __libc_valloc, __libc_pvalloc,
 * __posix_memalign) and, in every build, the C++ operators new / delete
 * (all variants) through their mangled names; none needs a declaration.
 */

/**
 * @brief Return the usable size of a block allocated by malloc.
 *
 * This function returns the usable size of a block of memory
 * allocated by `malloc`, `calloc`, or `realloc`. The usable size may be larger than the requested size, and it is guaranteed
 * to be at least as large as the size requested during allocation.
 *
 * @param ptr Pointer to the allocated block.
 * @return The usable size of the block, or 0 if ptr is NULL.
 */
_hmDeclExport size_t	malloc_usable_size(void *ptr)
		_hmAttrNoexcept;

/** @brief Usable size of a block (macOS). Same as husableSize. */
_hmDeclExport size_t	malloc_size(const void *p)
		_hmAttrNoexcept;

/** @brief Usable size a request of `size` bytes gets (macOS). Same as hgoodSize. */
_hmDeclExport size_t	malloc_good_size(size_t size)
		_hmAttrNoexcept;

/** @brief Release a block (legacy Unix names). Same as free. */
_hmDeclExport void	cfree(void *p)
		_hmAttrNoexcept;
_hmDeclExport void	vfree(void *p)
		_hmAttrNoexcept;

/** @brief Aligned allocation (Windows CRT name). Same as hmallocAligned. */
_hmDeclNodiscard _hmDeclExport _hmDeclRestrict
void	*_aligned_malloc(size_t size, size_t alignment)
		_hmAttrNoexcept _hmAttrMalloc _hmAttrAllocSize(1) _hmAttrAllocAlign(2);

/* ----- 5. Leak tracking ----- */

/**
 * @brief Classification of a leaked block.
 *
 * This enumeration defines the possible classifications for
 * leaked memory blocks. It is used in the leak detection and
 * reporting functionality of the hmalloc allocator.
 *
 * - DEFINITELY_LOST: The block is definitely lost and cannot be reached.
 * - INDIRECTLY_LOST: The block is indirectly lost,
 * 					  meaning it is reachable only through other lost blocks.
 * - POSSIBLY_LOST: The block is possibly lost,
 * 					  meaning it may be reachable but is not currently accessible.
 * - STILL_REACHABLE: The block is still reachable and can be accessed.
 * - SUPPRESSED: The block is suppressed and will not be reported as a leak.
 */
typedef enum e_hmLeakKind {
	HMLEAK_DEFINITELY_LOST	= 0,
	HMLEAK_INDIRECTLY_LOST	= 1,
	HMLEAK_POSSIBLY_LOST	= 2,
	HMLEAK_STILL_REACHABLE	= 3,
	HMLEAK_SUPPRESSED		= 4,
} t_hmLeakKind;

/* ----- One leaked block, as reported ----- */

typedef struct s_hmLeakRecord {
	const void		*ptr;		/* block base address */
	size_t			size;		/* requested size (from padding) */
	size_t			blockSize;	/* full block size including padding */
	t_hmLeakKind	kind;
	uintptr_t		ownerTid;	/* thread that allocated it */
	uintptr_t		arenaStart;	/* arena base (for context) */
	uintptr_t		pageStart;	/* page base (for context) */
	/* Stack trace captured at allocation time. */
	uintptr_t		stack[32];
	int				stackDepth;
} t_hmLeakRecord;

/**
 * @brief Enable stack trace capture at malloc() time.
 *
 * Called once at startup (or lazily on first malloc) from
 * `_hmallocAutoProcessInit`. Off by default; the call site is inside
 * the allocator's init path so it can be enabled at compile time
 * with -DHMALLOC_LEAK_TRACE=1 or at runtime via
 * `hmLeakSetTrace(true)`.
 *
 * Cost: 128 bytes of metadata per live block, plus one backtrace per
 * malloc. On a modern CPU this is a few microseconds per allocation;
 * it slows down alloc-heavy workloads by ~10x. Only useful for leak
 * hunts.
 */
void	hmLeakSetTrace(bool enable);

/**
 * @brief Record a malloc in the leak tracker.
 *
 * Called from `hmallocAllocInternal` *after* the block has been
 * allocated and its padding set, but *before* it is returned to the
 * caller. No-op when tracing is disabled or when the block is
 * immediately freed.
 *
 * @param ptr   User pointer (block start + padding).
 * @param size  Requested size (before padding).
 */
void	hmLeakOnAlloc(void *ptr, size_t size);

/**
 * @brief Record a free in the leak tracker.
 *
 * Removes the allocation from the live set. No-op when tracing is
 * disabled or when the pointer is unknown (interior pointer, reused
 * memory, etc.).
 *
 * @param ptr User pointer passed to free().
 */
void	hmLeakOnFree(void *ptr);

/**
 * @brief Print a Valgrind-style leak report on stderr.
 *
 * Walks every arena, classifies every block, resolves the stack
 * traces, and prints one section per classification, plus a summary
 * that is identical to Valgrind's. The output format is:
 *
 *     ==12345== LEAK SUMMARY:
 *     ==12345==    definitely lost: 32 bytes in 2 blocks
 *     ==12345==    indirectly lost: 0 bytes in 0 blocks
 *     ==12345==      possibly lost: 128 bytes in 1 blocks
 *     ==12345==    still reachable: 4096 bytes in 3 blocks
 *     ==12345==         suppressed: 0 bytes in 0 blocks
 *
 * The function takes the global allocator lock while walking, then
 * releases it before printing (so symbolization can allocate).
 *
 * Requires `HMALLOC_LEAK_TRACE=1` for the stack traces to be
 * meaningful. Without it, blocks are still classified but the
 * "at 0x...: ..." lines are omitted.
 */
void	hmReportLeaks(void);

/* ----- C23 sized deallocation ----- */

/**
 * @brief Release a block whose requested size is known.
 *
 * @param ptr  Block to free, or NULL.
 * @param size Size passed to the allocation function.
 */
void	free_sized(void *ptr, size_t size);

/**
 * @brief Release an aligned block whose alignment and size are known.
 *
 * @param ptr       Block to free, or NULL.
 * @param alignment Alignment passed to aligned_alloc().
 * @param size      Size passed to aligned_alloc().
 */
void	free_aligned_sized(void *ptr, size_t alignment, size_t size);

/* ----- 6. Statistics types ----- */

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
 * @brief Maximum size covered by the direct allocation table.
 *
 * Allocations under this size follow a "fast-path" allocation strategy using a direct index table.
 * Larger allocations use the generic allocation path.
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

/* ----- Chunk bins ----- */

/**
 * @brief Size class of a 512-slice chunk of an arena.
 *
 * Each chunk of the free-slice bitmap is reserved for one class of
 * allocation so that small pages do not fragment the space needed by
 * larger ones. Reported per class in t_hmallocStats::chunkBins.
 */
typedef enum e_hmallocChunkBin {
	HMALLOC_CBIN_SMALL	= 0,	/* slice count == 1 */
	HMALLOC_CBIN_OTHER	= 1,	/* any other count up to a chunk */
	HMALLOC_CBIN_MEDIUM	= 2,	/* slice count == 8 */
	HMALLOC_CBIN_LARGE	= 3,	/* slice count == 64 */
	HMALLOC_CBIN_HUGE	= 4,	/* slice count > 512 */
	HMALLOC_CBIN_NONE	= 5,	/* no class yet (chunk completely free) */
	HMALLOC_CBIN_COUNT	= 6
} t_hmallocChunkBin;

/* ----- Stats snapshot ----- */

/* Increased on every incompatible change of t_hmallocStats. */
# define HMALLOC_STATS_VERSION	2

/** @brief A quantity over time (bytes, pages, threads...). */
typedef struct s_hmallocStatCount {
	int64_t	total;		/* Sum of all increases */
	int64_t	peak;		/* Highest `current` ever reached */
	int64_t	current;	/* Current value */
} t_hmallocStatCount;

/** @brief A counter that only increases. */
typedef struct s_hmallocStatCounter {
	int64_t	total;
} t_hmallocStatCounter;

/*
 * The statistic fields, in mimalloc order. HMALLOC_STAT_COUNT fields are
 * t_hmallocStatCount, HMALLOC_STAT_COUNTER fields t_hmallocStatCounter.
 * Defining both macros lets code iterate over every field.
 */
# define HMALLOC_STAT_FIELDS() \
	HMALLOC_STAT_COUNT(pages)				/* pages in use */ \
	HMALLOC_STAT_COUNT(reserved)			/* reserved bytes */ \
	HMALLOC_STAT_COUNT(committed)			/* committed bytes */ \
	HMALLOC_STAT_COUNTER(reset)				/* reset bytes */ \
	HMALLOC_STAT_COUNTER(purged)			/* purged bytes */ \
	HMALLOC_STAT_COUNT(pageCommitted)		/* committed bytes inside pages */ \
	HMALLOC_STAT_COUNT(pagesAbandoned)		/* abandoned pages */ \
	HMALLOC_STAT_COUNT(threads)				/* threads */ \
	HMALLOC_STAT_COUNT(allocated)			/* allocated bytes */ \
	HMALLOC_STAT_COUNTER(mallocCount)		/* malloc calls */ \
	HMALLOC_STAT_COUNTER(freeCount)			/* free calls */ \
	HMALLOC_STAT_COUNTER(freed)				/* freed bytes */ \
	HMALLOC_STAT_COUNTER(mallocRequested)	/* requested bytes */ \
	\
	HMALLOC_STAT_COUNTER(mmapCalls) \
	HMALLOC_STAT_COUNTER(commitCalls) \
	HMALLOC_STAT_COUNTER(resetCalls) \
	HMALLOC_STAT_COUNTER(purgeCalls) \
	HMALLOC_STAT_COUNTER(arenaCount)		/* arenas created */ \
	HMALLOC_STAT_COUNTER(mallocGuardedCount)	/* allocations with guard pages */ \
	\
	/* internal statistics */ \
	HMALLOC_STAT_COUNTER(arenaRollbackCount) \
	HMALLOC_STAT_COUNTER(arenaPurges) \
	HMALLOC_STAT_COUNTER(pagesExtended)		/* free-list extensions */ \
	HMALLOC_STAT_COUNTER(pagesRetire)		/* pages retired */ \
	HMALLOC_STAT_COUNTER(pageSearches)		/* pages examined to find a free page */ \
	HMALLOC_STAT_COUNTER(pageSearchesCount)	/* searches for a free page */ \
	HMALLOC_STAT_COUNT(heaps) \
	HMALLOC_STAT_COUNT(theaps) \
	HMALLOC_STAT_COUNT(pagesOsAbandoned)	/* abandoned pages outside arenas */ \
	HMALLOC_STAT_COUNT(pagesOsAllocated)	/* pages allocated outside arenas */ \
	HMALLOC_STAT_COUNTER(pagesReclaimOnAlloc) \
	HMALLOC_STAT_COUNTER(pagesReclaimOnFree) \
	HMALLOC_STAT_COUNTER(pagesReabandonFull) \
	HMALLOC_STAT_COUNTER(pagesUnabandonBusyWait) \
	HMALLOC_STAT_COUNTER(pagesUnownCasRetry) \
	HMALLOC_STAT_COUNTER(heapsDeleteWait) \
	HMALLOC_STAT_COUNTER(pagesStatUpdates) \
	HMALLOC_STAT_COUNTER(pagesStatUpdateCount) \
	HMALLOC_STAT_COUNTER(profileSamples)	/* sampled (profiled) allocations */ \
	\
	HMALLOC_STAT_COUNT(mallocNormal)		/* bytes in blocks <= HMALLOC_LARGE_MAX_OBJ_SIZE */ \
	HMALLOC_STAT_COUNT(mallocHuge)			/* bytes in huge blocks */ \
	HMALLOC_STAT_COUNTER(mallocNormalCount) \
	HMALLOC_STAT_COUNTER(mallocHugeCount)

# define HMALLOC_STAT_COUNT(stat)	t_hmallocStatCount		stat;
# define HMALLOC_STAT_COUNTER(stat)	t_hmallocStatCounter	stat;

/**
 * @brief Snapshot of the allocator statistics.
 *
 * Initialise it with hmStatsInit (sets `size` and `version`, which the
 * getters check). When HMALLOC_STATS == 0 the getters fail and nothing
 * is collected, but the type is always available.
 */
typedef struct s_hmallocStats {
	size_t	size;		/* = sizeof(t_hmallocStats) */
	size_t	version;	/* = HMALLOC_STATS_VERSION */

	__attribute__((aligned(8))) HMALLOC_STAT_FIELDS()

	/* Room for future fields without changing the layout. */
	t_hmallocStatCount		_statReserved[4];
	t_hmallocStatCounter	_statCounterReserved[4];

	/* Size segregated statistics. */
	t_hmallocStatCount	mallocBins[HMALLOC_BIN_HUGE + 1];	/* allocations per size bin */
	t_hmallocStatCount	pageBins[HMALLOC_BIN_HUGE + 1];		/* pages per size bin */
	t_hmallocStatCount	chunkBins[HMALLOC_CBIN_COUNT];		/* arena chunks per class */
} t_hmallocStats;

# undef HMALLOC_STAT_COUNT
# undef HMALLOC_STAT_COUNTER

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

/** @brief Declare and initialise a t_hmallocStats variable. */
# define hmStatsDecl(name)	t_hmallocStats name; hmStatsInit(&name)

/* ----- 7. Heaps ----- */

/**
 * @brief Opaque handle to an allocator heap.
 *
 * A heap is an independent collection of pages. The allocator
 * ships with a default heap; the caller may create additional
 * ones to isolate unrelated workloads and free them at once.
 */
typedef struct s_hmallocHeap	t_hmallocHeap;

/**
 * @brief Opaque handle to a thread-local heap: the part of a heap owned
 *        by one thread.
 */
typedef struct s_hmallocTheap	t_hmallocTheap;

/**
 * @brief Abstract identifier of a sub-process (a set of heaps and arenas).
 */
typedef struct s_hmSubprocId {
	void	*_hmSubprocId;
} t_hmSubprocId;

/** @brief The main sub-process. */
_hmDeclExport
t_hmSubprocId	hmSubprocMain(void)
		_hmAttrNoexcept;

/** @brief The main (default) heap. */
_hmDeclNodiscard _hmDeclExport
t_hmallocHeap	*hmHeapMain(void)
		_hmAttrNoexcept;

/** @brief The calling thread's theap of the main heap. */
_hmDeclExport
t_hmallocTheap	*hmTheapGetDefault(void)
		_hmAttrNoexcept;

/**
 * @brief Create a new, empty heap.
 *
 * @return Pointer to the new heap, or NULL on failure.
 */
_hmDeclNodiscard _hmDeclExport
t_hmallocHeap	*hmHeapNew(void);

/**
 * @brief Delete a heap, keeping live blocks valid.
 *
 * Live blocks are transferred to the default heap so that
 * live pointers remain valid. Calling on the default heap is
 * a no-op.
 *
 * @param heap Heap to delete, or NULL.
 */
_hmDeclExport
void	hmHeapDelete(t_hmallocHeap *heap);

/**
 * @brief Destroy a heap, releasing every page.
 *
 * Unlike hmHeapDelete, this releases even pages with live
 * blocks. Use only at teardown.
 *
 * @param heap Heap to destroy, or NULL.
 */
_hmDeclExport
void	hmHeapDestroy(t_hmallocHeap *heap);

/**
 * @brief Run garbage collection on a heap.
 *
 * @param heap  Heap to collect, or NULL for the default heap.
 * @param force If true, release pages kept for reuse.
 */
_hmDeclExport
void	hmHeapCollect(t_hmallocHeap *heap, bool force);

/**
 * @brief Allocate from a specific heap.
 *
 * @param heap Heap to allocate from, or NULL for the default.
 * @param size Size in bytes.
 * @return Pointer to the block, or NULL on failure.
 */
_hmDeclNodiscard _hmDeclExport _hmDeclRestrict
void	*hmHeapMalloc(t_hmallocHeap *heap, size_t size)
		_hmAttrNoexcept _hmAttrMalloc _hmAttrAllocSize(2);

/* ----- Heap of a block ----- */

/**
 * @brief Heap owning a block.
 *
 * @param p Any pointer.
 * @return The heap of the page holding `p`, or NULL if `p` was not
 *         allocated by hmalloc.
 */
_hmDeclNodiscard _hmDeclExport
t_hmallocHeap	*hmHeapOf(const void *p)
		_hmAttrNoexcept;

/**
 * @brief Whether `p` belongs to `heap` (NULL: the main heap).
 */
_hmDeclNodiscard _hmDeclExport
bool	hmHeapContains(const t_hmallocHeap *heap, const void *p)
		_hmAttrNoexcept;

/**
 * @brief Whether `p` points into memory managed by hmalloc (any heap).
 */
_hmDeclNodiscard _hmDeclExport
bool	hmAnyHeapContains(const void *p)
		_hmAttrNoexcept;

/* ----- Visiting blocks ----- */

/**
 * @brief An area of a heap: one page and its blocks.
 */
typedef struct s_hmHeapArea {
	void	*blocks;		/* Start of the blocks of the area */
	size_t	reserved;		/* Bytes reserved for this area (virtual) */
	size_t	committed;		/* Bytes of blocks currently available */
	size_t	used;			/* Number of allocated blocks */
	size_t	blockSize;		/* Usable size of each block */
	size_t	fullBlockSize;	/* Size of a block including padding and metadata */
	void	*reserved1;		/* Internal */
} t_hmHeapArea;

/**
 * @brief Visitor called for each area, then for each live block.
 *
 * Called once per area with `block == NULL`, then (if blocks are
 * visited) once per allocated block. Return false to stop.
 */
typedef bool (_hmCdecl t_hmBlockVisitFun)(const t_hmallocHeap *heap,
										  const t_hmHeapArea *area,
										  void *block, size_t blockSize,
										  void *arg);

/**
 * @brief Visit the areas (and optionally blocks) of a heap.
 *
 * No other thread may use the heap meanwhile (thread exit excepted).
 * Pages of blocks allocated directly from the OS (larger than an
 * arena) are not visited.
 *
 * @param heap        Heap to visit, NULL for the main heap.
 * @param visitBlocks Also call `visitor` for every live block.
 * @return false if the visitor stopped the walk.
 */
_hmDeclExport
bool	hmHeapVisitBlocks(t_hmallocHeap *heap, bool visitBlocks,
						  t_hmBlockVisitFun *visitor, void *arg);

/**
 * @brief Like hmHeapVisitBlocks, but only the abandoned pages of the heap
 *        (pages whose owning thread has exited).
 */
_hmDeclExport
bool	hmHeapVisitAbandonedBlocks(t_hmallocHeap *heap, bool visitBlocks,
								   t_hmBlockVisitFun *visitor, void *arg);


/* ----- 8. Global helpers ----- */

/**
 * @brief Run garbage collection on the default heap.
 * @param force If true, release pages kept for reuse.
 */
_hmDeclExport
void	hmCollect(bool force)
		_hmAttrNoexcept;

/**
 * @brief Check whether the allocator owns the standard symbols.
 *
 * Useful in LD_PRELOAD scenarios.
 *
 * @return true if hmalloc is the active allocator.
 */
_hmDeclExport
bool	hmIsRedirected(void)
		_hmAttrNoexcept;

/**
 * @brief Query process-wide memory information.
 *
 * Any output pointer may be NULL to skip that value.
 *
 * @param elapsedMsecs   Monotonic timestamp (ms, since boot).
 * @param userMsecs      User CPU time (ms).
 * @param systemMsecs    System CPU time (ms).
 * @param currentRss     Current resident set size (bytes).
 * @param peakRss        Peak resident set size (bytes).
 * @param currentCommit  Current committed memory (bytes).
 * @param peakCommit     Peak committed memory (bytes).
 * @param pageFaults     Number of page faults.
 */
_hmDeclExport
void	hmProcessInfo(size_t *elapsedMsecs, size_t *userMsecs,
					  size_t *systemMsecs, size_t *currentRss,
					  size_t *peakRss, size_t *currentCommit,
					  size_t *peakCommit, size_t *pageFaults)
		_hmAttrNoexcept;

/* ----- 9. Callbacks ----- */

/**
 * @brief Callback used to emit a textual message.
 */
typedef void (_hmCdecl t_hmOutputFun)(const char *msg, void *arg);

/**
 * @brief Callback used to report an error.
 */
typedef void (_hmCdecl t_hmErrorFun)(int err, void *arg);

/**
 * @brief Callback invoked periodically to drive deferred frees.
 */
typedef void (_hmCdecl t_hmDeferredFreeFun)(bool force,
											unsigned long long heartbeat,
											void *arg);

/**
 * @brief Register the output callback.
 *
 * Passing NULL restores the default (writes to fd 2).
 */
_hmDeclExport
void	hmRegisterOutput(t_hmOutputFun *out, void *arg)
		_hmAttrNoexcept;

/**
 * @brief Register the error callback.
 *
 * Passing NULL restores the default behavior.
 */
_hmDeclExport
void	hmRegisterError(t_hmErrorFun *fun, void *arg)
		_hmAttrNoexcept;

/**
 * @brief Register the deferred-free callback.
 */
_hmDeclExport
void	hmRegisterDeferredFree(t_hmDeferredFreeFun *fn, void *arg)
		_hmAttrNoexcept;

/* ----- 10. 42 project ----- */

/**
 * @brief Print the state of all allocated blocks.
 *
 * Prints one line per allocated block, grouped by category
 * (TINY, SMALL, LARGE) and sorted by increasing address. The
 * format matches the 42 malloc subject exactly:
 *
 *     TINY : 0xA0000
 *     0xA0020 - 0xA004A : 42 bytes
 *     SMALL : 0xAD000
 *     ...
 *     Total : 52698 bytes
 *
 * Output goes to file descriptor 1.
 */
_hmDeclExport
void	show_alloc_mem(void);

/**
 * @brief Print the state of all allocated blocks, with a hexdump.
 *
 * Same as `show_alloc_mem`, with each block followed by a hexdump of its
 * first bytes (64 by default, `HMALLOC_VERBOSE=<n>` to change it):
 *
 *     0xA0020  48 65 6C 6C 6F 00 ...  |Hello.|
 *
 * In the 42 build with `HMALLOC_HISTORY=1`, the allocation history
 * (see `show_alloc_history`) follows.
 */
_hmDeclExport
void	show_alloc_mem_ex(void);

/**
 * @brief Print the last calls to malloc, calloc, realloc and free.
 *
 * 42 build only, with `HMALLOC_HISTORY=1`: the last 256 calls are kept
 * in a ring buffer and printed oldest first, one per line:
 *
 *     #12 malloc(42) = 0xA0020
 *     #13 realloc(0xA0020, 100) = 0xA0080
 *     #14 free(0xA0080)
 *
 * Without the option (or in other builds) only a notice is printed.
 */
_hmDeclExport
void	show_alloc_history(void);

/* ----- 11. Options ----- */

/**
 * @brief Runtime options of the allocator.
 *
 * Set at process start through environment variables
 * (`HMALLOC_<NAME>`) or at runtime with `hmOptionSet`.
 */
typedef enum e_hmallocOption {
	HMALLOC_OPT_SHOW_ERRORS = 0,
	HMALLOC_OPT_SHOW_STATS,
	HMALLOC_OPT_VERBOSE,
	HMALLOC_OPT_PURGE_DELAY,
	HMALLOC_OPT_PAGE_FULL_RETAIN,
	HMALLOC_OPT_GUARDED_SAMPLE_RATE,
	HMALLOC_OPT_PERTURB,
	HMALLOC_OPT_CHECK,
	HMALLOC_OPT_TRACE,
	HMALLOC_OPT_CHECK_LEAKS,
	HMALLOC_OPT_PAGE_RECLAIM_ON_FREE,			/* -1 off, 0 originating theap only, 1 also cross-thread */
	HMALLOC_OPT_PAGE_MAX_RECLAIM,				/* max pages per bin when reclaiming into the originating theap (-1: unlimited) */
	HMALLOC_OPT_PAGE_CROSS_THREAD_MAX_RECLAIM,	/* max pages per bin when reclaiming across threads */
	HMALLOC_OPT_GENERIC_COLLECT,				/* full theap collect every N generic allocations */
	HMALLOC_OPT_ARENA_PURGE_MULT,				/* arena purge delay = purge_delay * this */
	HMALLOC_OPT_ALLOW_THP,						/* 0 disable THP, 1 allow, 2 allow and purge by whole huge pages */
	HMALLOC_OPT_ARENA_PURGE_MIN_SIZE,			/* minimal purge size in KiB (0: huge page with THP, else OS page) */
	HMALLOC_OPT_ARENA_PURGE_IMMEDIATE_SIZE,		/* ranges of at least this size (KiB) are purged right away */
	HMALLOC_OPT_HISTORY,						/* 42 build: record the calls for show_alloc_history */
	HMALLOC_OPT_LAST
} t_hmallocOption;

/**
 * @brief Set an option.
 *
 * Out-of-range options are silently ignored.
 */
_hmDeclExport
void	hmOptionSet(t_hmallocOption opt, long value);

/**
 * @brief Get an option.
 *
 * Out-of-range options return 0.
 */
_hmDeclExport
long	hmOptionGet(t_hmallocOption opt);

/**
 * @brief Get an option holding a size, in bytes.
 *
 * Size options (HMALLOC_OPT_ARENA_PURGE_MIN_SIZE, ...) are stored in KiB;
 * this returns the value * 1024, saturated, and 0 for negative values.
 */
_hmDeclNodiscard _hmDeclExport
size_t	hmOptionGetSize(t_hmallocOption opt);

/* ----- 12. Statistics ----- */

# if HMALLOC_STATS

/**
 * @brief Fill a snapshot with the current statistics.
 *
 * `stats->size` and `stats->version` must be set beforehand
 * (the `hmStatsInit` helper from <hmStats.h> does this).
 *
 * @param stats Output structure.
 * @return true on success.
 */
_hmDeclNodiscard _hmDeclExport
bool	hmStatsGet(t_hmallocStats *stats)
		_hmAttrNoexcept;

/**
 * @brief Serialise the current statistics as JSON.
 *
 * @param bufSize Size of the output buffer.
 * @param buf     Output buffer.
 * @return Pointer to `buf` on success, or NULL if too small.
 */
_hmDeclNodiscard _hmDeclExport
char	*hmStatsGetJson(size_t bufSize, char *buf)
		_hmAttrNoexcept;

/**
 * @brief Send the current statistics to a callback.
 */
_hmDeclExport
void	hmStatsPrintOut(t_hmOutputFun *out, void *arg)
		_hmAttrNoexcept;

/**
 * @brief Print the current statistics to a stream.
 *
 * @param out Output stream, or NULL for the default
 *            (fd 2 in the standalone build).
 */
_hmDeclExport
void	hmStatsPrint(void *out)
		_hmAttrNoexcept;

/**
 * @brief Fill a snapshot with the statistics of a heap.
 */
_hmDeclNodiscard _hmDeclExport
bool	hmHeapStatsGet(t_hmallocHeap *heap, t_hmallocStats *stats)
		_hmAttrNoexcept;

/**
 * @brief Serialise the statistics of a heap as JSON.
 */
_hmDeclNodiscard _hmDeclExport
char	*hmHeapStatsGetJson(t_hmallocHeap *heap, size_t bufSize, char *buf)
		_hmAttrNoexcept;

/**
 * @brief Send the statistics of a heap to a callback.
 */
_hmDeclExport
void	hmHeapStatsPrintOut(t_hmallocHeap *heap,
							 t_hmOutputFun *out, void *arg)
		_hmAttrNoexcept;

/**
 * @brief Reset all statistics counters.
 */
_hmDeclExport
void	hmStatsReset(void)
		_hmAttrNoexcept;

/**
 * @brief Merge the statistics of the calling thread into the
 *        global counters.
 */
_hmDeclExport
void	hmStatsMerge(void)
		_hmAttrNoexcept;

/* ----- Sub-process, heap and theap statistics ----- */

/**
 * @brief Statistics of a sub-process aggregated with all its heaps.
 * @return false on a bad header (size/version) or subproc.
 */
_hmDeclNodiscard _hmDeclExport
bool	hmSubprocStatsGet(t_hmSubprocId subprocId, t_hmallocStats *stats)
		_hmAttrNoexcept;

/**
 * @brief Send the aggregated statistics of a sub-process to a callback
 *        (NULL: the default output).
 */
_hmDeclExport
void	hmSubprocStatsPrintOut(t_hmSubprocId subprocId,
							   t_hmOutputFun *out, void *arg)
		_hmAttrNoexcept;

/**
 * @brief Add the statistics of a heap (NULL: main heap) to its
 *        sub-process and clear them.
 */
_hmDeclExport
void	hmHeapStatsMergeToSubproc(t_hmallocHeap *heap);

/**
 * @brief Statistics of one theap, not yet merged into its heap.
 */
_hmDeclNodiscard _hmDeclExport
bool	hmTheapStatsGet(t_hmallocTheap *theap, t_hmallocStats *stats)
		_hmAttrNoexcept;

/**
 * @brief Add the statistics of a theap to its heap and clear them.
 */
_hmDeclExport
void	hmTheapStatsMergeToHeap(t_hmallocTheap *theap)
		_hmAttrNoexcept;

# else /* HMALLOC_STATS == 0 */

/*
 * Statistics are compiled out. Provide inline stubs so the
 * API still links. The function names remain reserved; call
 * with HMALLOC_STATS=1 if you need real numbers.
 */
static inline bool	hmStatsGet(t_hmallocStats *s)
	{ (void)s; return (false); }
static inline char	*hmStatsGetJson(size_t bufSize, char *buf)
	{ (void)bufSize; (void)buf; return (NULL); }
static inline void	hmStatsPrintOut(t_hmOutputFun *out, void *arg)
	{ (void)out; (void)arg; }
static inline void	hmStatsPrint(void *out)
	{ (void)out; }
static inline bool	hmHeapStatsGet(t_hmallocHeap *h, t_hmallocStats *s)
	{ (void)h; (void)s; return (false); }
static inline char	*hmHeapStatsGetJson(t_hmallocHeap *h, size_t n, char *b)
	{ (void)h; (void)n; (void)b; return (NULL); }
static inline void	hmHeapStatsPrintOut(t_hmallocHeap *h,
										t_hmOutputFun *out, void *arg)
	{ (void)h; (void)out; (void)arg; }
static inline void	hmStatsReset(void)				{}
static inline void	hmStatsMerge(void)				{}
static inline bool	hmSubprocStatsGet(t_hmSubprocId id, t_hmallocStats *s)
	{ (void)id; (void)s; return (false); }
static inline void	hmSubprocStatsPrintOut(t_hmSubprocId id,
										   t_hmOutputFun *out, void *arg)
	{ (void)id; (void)out; (void)arg; }
static inline void	hmHeapStatsMergeToSubproc(t_hmallocHeap *h)		{ (void)h; }
static inline bool	hmTheapStatsGet(t_hmallocTheap *t, t_hmallocStats *s)
	{ (void)t; (void)s; return (false); }
static inline void	hmTheapStatsMergeToHeap(t_hmallocTheap *t)		{ (void)t; }

# endif /* HMALLOC_STATS */

/* ----- 13. Profiling ----- */

/* Forward declaration so the API below is always legal. */
struct s_hmallocProfiler;

# if HMALLOC_PROFILE

/**
 * @brief Profiler descriptor.
 *
 * The caller fills this structure and passes it to
 * `hmProfile` / `hmHeapProfile` before calling
 * `hmProfilerStart`.
 */
struct s_hmallocProfiler {
	size_t	initialSampleRate;
	size_t	sampleRate;
	size_t	sampleCountdown;
	size_t	sampleCount;
	size_t	totalSamples;
	size_t	bytesSinceLastSample;

	void	*arg;
	void	(*onSample)(void *arg, const void *block,
						size_t requested, size_t usable);
	void	(*onStop)(void *arg);

	t_hmallocHeap	*heap;
	bool			active;
};

typedef struct s_hmallocProfiler	t_hmallocProfiler;

/**
 * @brief Attach a profiler to a heap.
 *
 * @param heap Heap to profile, or NULL for the default heap.
 * @param prof Profiler descriptor.
 * @return true on success.
 */
_hmDeclExport
bool	hmHeapProfile(t_hmallocHeap *heap, t_hmallocProfiler *prof);

/**
 * @brief Detach the profiler from a heap.
 */
_hmDeclExport
void	hmHeapProfileDisable(t_hmallocHeap *heap);

/**
 * @brief Attach a profiler to the default heap.
 *
 * Equivalent to `hmHeapProfile(NULL, prof)`.
 */
_hmDeclExport
bool	hmProfile(t_hmallocProfiler *prof);

/**
 * @brief Start a profiler.
 *
 * The profiler must already have been attached to a heap.
 */
_hmDeclExport
bool	hmProfilerStart(t_hmallocProfiler *prof);

/**
 * @brief Stop a profiler.
 */
_hmDeclExport
bool	hmProfilerStop(t_hmallocProfiler *prof);

# else /* HMALLOC_PROFILE == 0 */

typedef struct s_hmallocProfiler	t_hmallocProfiler;

static inline bool	hmHeapProfile(t_hmallocHeap *h, t_hmallocProfiler *p)
	{ (void)h; (void)p; return (false); }
static inline void	hmHeapProfileDisable(t_hmallocHeap *h)
	{ (void)h; }
static inline bool	hmProfile(t_hmallocProfiler *p)
	{ (void)p; return (false); }
static inline bool	hmProfilerStart(t_hmallocProfiler *p)
	{ (void)p; return (false); }
static inline bool	hmProfilerStop(t_hmallocProfiler *p)
	{ (void)p; return (false); }

# endif /* HMALLOC_PROFILE */

/* ----- Inline qualifier fallback ----- */

# ifndef __HAJ_INLINE
#  if defined(__HAJ_C_VERSION) && __HAJ_C_VERSION >= 199901L
#   define __HAJ_INLINE	inline
#  elif defined(__HAJ_COMPILER_GNULIKE)
#   define __HAJ_INLINE	__inline__
#  elif defined(__HAJ_COMPILER_MSVC)
#   define __HAJ_INLINE	__inline
#  else
#   define __HAJ_INLINE
#  endif
# endif

# ifdef __cplusplus
}
# endif

#endif /* HMALLOC_H */
