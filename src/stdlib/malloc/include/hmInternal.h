/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file hmInternal.h
 * @brief Internal definitions and utilities for the hajmalloc allocator.
 * @Created: 2026/10/05 16:24:44 by Moutig
 * @Updated: 2026/10/20 14:20:16 by Moutig
 *
 * This header file contains internal definitions, macros, and utility functions
 * used by the hajmalloc allocator. It includes alignment utilities, overflow checks,
 * and assertion mechanisms for debugging purposes.
 */

#ifndef HMALLOC_INTERNAL_H
# define HMALLOC_INTERNAL_H

# include <stdarg.h>
# include <stdbool.h>
# include <stddef.h>
# include <stdint.h>

# include "../../../../include/hmConfig.h"

# include "../../../../include/malloc.h"

# include "hmAtomic.h"
# include "hmPrim.h"
# include "hmTrack.h"

/* ----- Inline / noinline / noreturn / hidden ----- */

# if defined(_MSC_VER)
#  define _hmDeclForceinline	__forceinline
#  define _hmDeclNoinline		__declspec(noinline)
#  define _hmDeclNoreturn		__declspec(noreturn)
#  define _hmDeclHidden
#  define _hmInlineUnused		__inline
# elif defined(__GNUC__) || defined(__clang__)
#  define _hmDeclForceinline	__attribute__((always_inline)) inline
#  define _hmDeclNoinline		__attribute__((noinline))
#  define _hmDeclNoreturn		__attribute__((noreturn))
#  define _hmDeclHidden			__attribute__((visibility("hidden")))
#  define _hmInlineUnused		__attribute__((unused))
# else
#  define _hmDeclForceinline	inline
#  define _hmDeclNoinline
#  define _hmDeclNoreturn
#  define _hmDeclHidden
#  define _hmInlineUnused
# endif

/* ----- Branch hints ----- */

# if defined(__GNUC__) || defined(__clang__)
#  define hmUnlikely(x)	(__builtin_expect(!!(x), 0))
#  define hmLikely(x)	(__builtin_expect(!!(x), 1))
# else
#  define hmUnlikely(x)	(x)
#  define hmLikely(x)	(x)
# endif

/* ----- Assertions ----- */

# if HMALLOC_DEBUG > 0
_hmDeclNoreturn _hmDeclHidden
void	_hmallocAssertFail(const char *expr, const char *file, unsigned line, const char *func);
#  define hmallocAssert(x)	((x) ? (void)0 : _hmallocAssertFail(#x, __FILE__, __LINE__, __func__))
# else
#  define hmallocAssert(x)	((void)0)
# endif

# if HMALLOC_DEBUG > 1
#  define hmallocAssertInternal	hmallocAssert
# else
#  define hmallocAssertInternal(x)	((void)0)
# endif

/* ----- Options ----- */

# ifndef HMALLOC_OPTIONS_MAX
#  define HMALLOC_OPTIONS_MAX	16
# endif

/**
 * @brief Thread-local storage for the memory allocator.
 *
 * This struct holds thread-local data for the memory allocator, including
 * a lock for thread safety, a key for the thread-local heap, and other
 * relevant information.
 */
typedef struct s_hmallocTs {
	t_hmallocLock	lock;			/* Lock for thread-safe access to the thread-local data */
	pthread_key_t	theapKey;		/* Key for the thread-local heap (theap) */
	pthread_once_t	theapKeyOnce;	/* Once control for initializing the theapKey */
	bool			theapKeyValid;	/* Indicates if the theapKey has been initialized and is valid */

	/* Bin-range cache (moved here from theap.c statics). */
	uint16_t		binRangeStart[HMALLOC_BIN_COUNT];
	uint16_t		binRangeEnd[HMALLOC_BIN_COUNT];
	pthread_once_t	binRangeOnce;	/* Once control for initializing the bin-range cache */
} t_hmallocTs;

/**
 * @brief Global thread-local storage for the memory allocator.
 *
 * This variable holds the global thread-local storage for the memory allocator,
 * allowing for efficient access to thread-specific data across different threads.
 */
extern t_hmallocTs	g_hmallocTs;

/**
 * @brief Global state for the memory allocator.
 *
 * This variable holds the global state for the memory allocator, including
 * configuration options, memory management data, and other relevant information.
 */
typedef struct s_hmallocGlobal {
	t_hmallocOsMemConfig	memConfig;		/* Operating system memory configuration */
	size_t					pageSize;		/* Size of a memory page in bytes */

	t_hmallocSubproc	subprocMain;/* Main subprocess */
	t_hmallocHeap		heapMain;	/* Main heap for the allocator */
	t_hmallocTheap		theapEmpty;	/* Sentinel theap for new threads that have never allocated */
	t_hmallocPage		pageEmpty;	/* Sentinel page for new threads that have never allocated */

	t_hmallocArena		*arenas[3];			/* Array of pointers to arenas for each category (tiny, small, large) */
	_Atomic(t_hmallocArena *)	spareLarge;	/* Pointer to a spare large arena for reuse */

	void	*metaCur;	/* Current metadata allocation pointer */
	size_t	metaLeft;	/* Remaining bytes in the current metadata allocation */
	void	*metaFree;	/* Pointer to the free list of metadata allocations */

	size_t				addrLimit;	/* Address limit for memory allocations (used for security and debugging) */
	_Atomic size_t		mappedBytes;/* Total bytes mapped from the OS */

	t_hmOutputFun		*outFun;		/* Output function for logging and error messages */
	void				*outArg;		/* Argument for the output function */
	t_hmErrorFun		*errFun;		/* Error handling function for the allocator */
	void				*errArg;		/* Argument for the error handling function */
	t_hmDeferredFreeFun	*deferredFun;	/* Deferred free function for the allocator */
	void				*deferredArg;	/* Argument for the deferred free function */

	_Atomic long	options[HMALLOC_OPTIONS_MAX];
	bool			initialized;	/* Indicates if the allocator has been initialized */

# if HMALLOC_STATS
	t_hmallocStats	statsSubproc;
	t_hmallocStats	statsHeapMain;
# endif
} t_hmallocGlobal;

/**
 * @brief Global state for the memory allocator.
 *
 * This variable holds the global state for the memory allocator, including
 * configuration options, memory management data, and other relevant information.
 */
extern t_hmallocGlobal	g_hmalloc;

/* ----- Alignment helpers ----- */

static __HAJ_INLINE _hmInlineUnused
uintptr_t	hmallocAlignUp(uintptr_t sz, size_t a)
{
	const uintptr_t	m = (uintptr_t)a - 1;

	return ((sz + m) & ~m);
}


static __HAJ_INLINE _hmInlineUnused
size_t	hmallocMin(size_t a, size_t b)
{
	return (a <= b ? a : b);
}

static __HAJ_INLINE _hmInlineUnused
void	*hmallocPtrDecode(const void *null, t_hmEncoded x, const uintptr_t *keys)
{
	const uintptr_t	k1 = keys[0];
	const uintptr_t	k2 = keys[1];
	void			*p;

	p = (void *)(hmallocRotr(x - k1, k1) ^ k2);
	return (p == null ? NULL : p);
}

static __HAJ_INLINE _hmInlineUnused
t_hmEncoded	hmallocPtrEncode(const void *null, const void *p, const uintptr_t *keys)
{
	const uintptr_t	k1 = keys[0];
	const uintptr_t	k2 = keys[1];
	const uintptr_t	x = (uintptr_t)(p == NULL ? null : p);

	return ((t_hmEncoded)(hmallocRotl(x ^ k2, k1) + k1));
}

static __HAJ_INLINE _hmInlineUnused
uint8_t	*hmallocPageStart(const t_hmallocPage *p)
{
	return ((uint8_t *)p + p->pageOffset);
}

static __HAJ_INLINE _hmInlineUnused
t_hmallocBlock *hmallocBlockNext(const t_hmallocPage *page, const t_hmallocBlock *b)
{
# if HMALLOC_ENCODE_FREELIST
	return hmallocPtrDecode(page, b->next, page->keys);
# else
	(void)page;
	return ((t_hmallocBlock *)(uintptr_t)b->next);
# endif
}

static __HAJ_INLINE _hmInlineUnused
void hmallocBlockSetNext(const t_hmallocPage *page, t_hmallocBlock *b, const t_hmallocBlock *next)
{
# if HMALLOC_ENCODE_FREELIST
	b->next = hmallocPtrEncode(page, next, page->keys);
# else
	(void)page;
	b->next = (t_hmEncoded)(uintptr_t)next;
# endif
}

/**
 * @struct hmFmtState
 * @brief State for formatted output.
 *
 * This struct holds the state for formatted output operations, including
 * the output buffer, its capacity, and the current position in the buffer.
 */
struct hmFmtState {
	char	*buf;	/* Output buffer */
	size_t	cap;	/* Capacity of the output buffer */
	size_t	pos;	/* Current position in the output buffer */
};

/**
 * @brief Format a string into a buffer.
 *
 * This function formats a string into the provided buffer, using the specified
 * format string and arguments.
 *
 * @param buf The output buffer.
 * @param cap The capacity of the output buffer.
 * @param fmt The format string.
 * @param ap The arguments for the format string.
 * @return The number of characters written to the buffer, or (size_t)-1 if an error occurred.
 */
size_t	hmallocFmt(char *buf, size_t cap, const char *fmt, va_list ap);

/**
 * @brief Format a string into a buffer with a state.
 *
 * This function formats a string into the provided buffer, using the specified
 * format string and arguments, and updates the state of the output.
 *
 * @param st The state for formatted output.
 * @param fmt The format string.
 * @param ap The arguments for the format string.
 */
void	hmallocFmtInto(struct hmFmtState *st, const char *fmt, va_list ap);

/**
 * @brief Convert an unsigned integer to a string.
 *
 * This function converts an unsigned integer value to a string representation
 * in the specified base, and writes it into the provided buffer.
 *
 * @param buf The output buffer.
 * @param cap The capacity of the output buffer.
 * @param v The unsigned integer value to convert.
 * @param base The base for conversion (e.g., 10 for decimal, 16 for hexadecimal).
 * @param upper Whether to use uppercase letters for bases greater than 10.
 * @return The number of characters written to the buffer, or (size_t)-1 if an error occurred.
 */
size_t	hmallocUtoa(char *buf, size_t cap, uint64_t v, unsigned base, bool upper);

/**
 * @brief Initialize the hmalloc options.
 *
 * This function initializes the hmalloc options.
 */
void	_hmallocOptionsInit(void);

/**
 * @brief Perform post-initialization of the hmalloc options.
 *
 * This function performs any necessary post-initialization steps for the
 * hmalloc options after they have been set.
 */
void	_hmallocOptionsPostInit(void);

/**
 * @brief Emit an error message.
 *
 * This function emits an error message using the registered error callback
 * or the default error handling mechanism.
 *
 * @param err The error code.
 * @param fmt The format string for the error message.
 * @param ... The arguments for the format string.
 */
void	_hmallocErrorMessage(int err, const char *fmt, ...);

/**
 * @brief Emit a warning message.
 *
 * This function emits a warning message using the registered output callback
 * or the default output mechanism.
 *
 * @param fmt The format string for the warning message.
 * @param ... The arguments for the format string.
 */
void	_hmallocWarningMessage(const char *fmt, ...);

/**
 * @brief Emit a verbose message.
 *
 * This function emits a verbose message using the registered output callback
 * or the default output mechanism. Verbose messages are typically used for
 * debugging and informational purposes.
 *
 * @param fmt The format string for the verbose message.
 * @param ... The arguments for the format string.
 */
void	_hmallocVerboseMessage(const char *fmt, ...);

/**
 * @brief Initialize the operating system memory management.
 *
 * This function initializes the operating system memory management for the
 * hmalloc allocator, including setting up any necessary data structures and
 * configurations.
 */
void	_hmallocOsInit(void);

/**
 * @brief Get the operating system page size.
 *
 * This function returns the size of a memory page as defined by the operating
 * system. The page size is used for memory allocation and management within
 * the hmalloc allocator.
 *
 * @return The size of a memory page in bytes.
 */
size_t	_hmallocOsPageSize(void);

/**
 * @brief Allocate memory from the operating system.
 *
 * This function allocates a contiguous block of memory from the operating
 * system, with the specified size and alignment. The allocated memory is
 * tracked for later deallocation.
 *
 * @param size The size of the memory block to allocate in bytes.
 * @param alignment The alignment requirement for the allocated memory.
 * @param memid Pointer to a structure to receive information about the allocated memory.
 * @return Pointer to the allocated memory block, or NULL on failure.
 */
void	*_hmallocMetaZalloc(t_hmallocSubproc *subproc, size_t size, t_hmallocMemId *memid);

/**
 * @brief Free memory allocated from the operating system.
 *
 * This function frees a previously allocated block of memory, returning it
 * to the operating system. The memory must have been allocated with
 * _hmallocMetaZalloc.
 *
 * @param subproc The sub-process context.
 * @param p Pointer to the memory block to free.
 * @param memid The ID of the memory block to free.
 */
void	_hmallocMetaFree(t_hmallocSubproc *subproc, void *p, t_hmallocMemId memid);


/**
 * @brief Initialize the memory allocator for the process.
 *
 * This function performs any necessary initialization steps for the
 * hmalloc allocator at the process level, including setting up global
 * state and configurations.
 */
void	_hmallocAutoProcessInit(void);


/**
 * @brief Initialize the thread-local data for the memory allocator.
 *
 * This function initializes the thread-local data for the hmalloc allocator,
 * including setting up any necessary data structures and configurations.
 *
 * @return Pointer to the initialized thread-local data, or NULL on failure.
 */
t_hmallocTheap	*_hmallocThreadInit(void);

/**
 * @brief Perform cleanup for the thread-local data of the memory allocator.
 *
 * This function performs any necessary cleanup steps for the thread-local
 * data of the hmalloc allocator when a thread is exiting, including releasing
 * resources and finalizing state.
 *
 * @param theap Pointer to the thread-local heap to clean up.
 */
void			_hmallocThreadDone(t_hmallocTheap *theap);

# if HMALLOC_STATS
void	_hmallocStatsInit(void);
void	_hmallocStatsMergeInto(t_hmallocStats *to, t_hmallocStats *from);
# else
#  define _hmallocStatsInit()					((void)0)
#  define _hmallocStatsMergeInto(to, from)		((void)0)
# endif

void	_hmallocRandomInit(t_hmallocTld *tld);
size_t	_hmallocRandomNext(t_hmallocTld *tld);

#endif /* HMALLOC_INTERNAL_H */
