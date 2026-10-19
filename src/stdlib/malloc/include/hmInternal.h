/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file hmInternal.h
 * @brief Internal helpers, the single global state, and the
 *        single thread-safety state.
 * @Created: 2026/10/05 16:24:44 by Moutig
 * @Updated: 2026/10/16 09:00:24 by Moutig
 *
 * The 42 subject allows exactly two global variables: one to
 * manage the allocations, one for thread-safety. They live
 * here as `g_hmalloc` and `g_hmallocTs`.
 *
 * Anything that is purely per-thread (the current theap
 * pointer, the thread id helper) is HMALLOC_THREAD_LOCAL and
 * therefore not counted as a global by the subject. See
 * hmPrimTls.h.
 *
 * The TLS key and the pthread_once flag live in g_hmallocTs
 * because they exist only to register the thread-exit
 * destructor and are, by nature, thread-safety plumbing.
 */

#ifndef HMALLOC_INTERNAL_H
# define HMALLOC_INTERNAL_H

# include "hmAtomic.h"
# include "hmPrim.h"
# include "hmPrimTls.h"
# include "hmTrack.h"
#include "../../../../include/malloc.h"
# include <stdarg.h>

/* ----- Default debug level ----- */

# ifndef HMALLOC_DEBUG
#  define HMALLOC_DEBUG	0
# endif

/* ----- Inline / noinline / noreturn / hidden ----- */

# if defined(_MSC_VER)
#  define _hmDeclForceinline	__forceinline
#  define _hmDeclNoinline		__declspec(noinline)
#  define _hmDeclNoreturn		__declspec(noreturn)
#  define _hmDeclHidden
# elif defined(__GNUC__) || defined(__clang__)
#  define _hmDeclForceinline	__attribute__((always_inline)) inline
#  define _hmDeclNoinline		__attribute__((noinline))
#  define _hmDeclNoreturn		__attribute__((noreturn))
#  define _hmDeclHidden			__attribute__((visibility("hidden")))
# else
#  define _hmDeclForceinline	inline
#  define _hmDeclNoinline
#  define _hmDeclNoreturn
#  define _hmDeclHidden
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
void	_hmallocAssertFail(const char *expr, const char *file,
						   unsigned line, const char *func);
#  define hmallocAssert(x)	((x) ? (void)0 : _hmallocAssertFail(#x, __FILE__, __LINE__, __func__))
# else
#  define hmallocAssert(x)
# endif


_hmDeclNoreturn _hmDeclHidden
void	_hmallocAssertFail(const char *expr, const char *file,
					   unsigned line, const char *func);

# if HMALLOC_DEBUG > 1
#  define hmallocAssertInternal		hmallocAssert
# else
#  define hmallocAssertInternal(x)
# endif

/* ----- Options ----- */

# ifndef HMALLOC_OPTIONS_MAX
#  define HMALLOC_OPTIONS_MAX	16
# endif

/* ----- Thread-safety state ----- */

/**
 * @brief The single global state dedicated to thread safety.
 *
 * The 42 subject allows one global variable for the
 * thread-safe aspect of the allocator. Everything that is
 * needed to make the allocator safe under concurrency lives
 * here: the global lock, the TLS key used by the thread-exit
 * destructor, and the once flag that drives its creation.
 *
 * Fields that are purely per-thread (the current theap
 * pointer, the thread id helper) are HMALLOC_THREAD_LOCAL and
 * are not counted as globals by the subject. See hmPrimTls.h.
 */
typedef struct s_hmallocTs {
	t_hmallocLock	lock;			/**< Global spinlock protecting the allocator state. */
	pthread_key_t	theapKey;		/**< TLS key used by the thread-exit destructor. */
	pthread_once_t	theapKeyOnce;	/**< Once flag that drives theapKey creation. */
	bool			theapKeyValid;	/**< True once theapKey has been created successfully. */
} t_hmallocTs;

/**
 * @brief The single global instance dedicated to thread safety.
 */
extern t_hmallocTs	g_hmallocTs;


/* ----- Allocation state ----- */

/**
 * @brief The single global state dedicated to allocation.
 *
 * The 42 subject allows one global variable for the allocation
 * bookkeeping. Everything the allocator needs to remember
 * outside of the per-thread theaps lives here.
 *
 * All fields are zero-initialised statically. The first call
 * to the allocator triggers _hmallocAutoProcessInit() which
 * fills in the fields that require a syscall.
 */
typedef struct s_hmallocGlobal {
	t_hmallocOsMemConfig	memConfig;		/**< Filled by hmallocPrimMemInit. */
	size_t					pageSize;		/**< Cached page size; 0 until queried. */
	int						numaNodeCount;	/**< Number of NUMA nodes; 0 until discovered. */

	t_hmallocSubproc	subprocMain;	/**< The only sub-process. */
	t_hmallocHeap		heapMain;		/**< The default heap. */
	t_hmallocTheap		theapEmpty;		/**< Sentinel for "not yet initialised". */
	t_hmallocPage		pageEmpty;		/**< Sentinel for "no free block". */

	t_hmallocArena		*arenas[3];		/**< Arena chain heads by category. */
	_Atomic(t_hmallocArena *) spareLarge;

	_Atomic(void *)		*pageMapTop;	/**< Page map (ptr -> page). */

	void				*metaCur;		/**< Next free byte in the current metadata slab. */
	size_t				metaLeft;		/**< Bytes remaining in the current slab. */
	void				*metaFree;		/**< Free-list of recycled metadata blocks. */

	size_t				addrLimit;		/**< Maximum addressable size, SIZE_MAX if unlimited. */
	_Atomic size_t		mappedBytes;	/**< Total bytes mmap'd for the allocator. */

	t_hmOutputFun		*outFun;		/**< Output callback. */
	void				*outArg;
	t_hmErrorFun		*errFun;		/**< Error callback. */
	void				*errArg;
	t_hmDeferredFreeFun	*deferredFun;	/**< Deferred-free callback. */
	void				*deferredArg;

	long				options[HMALLOC_OPTIONS_MAX];	/**< Runtime options. */
	size_t				totalAlloc;		/**< Bytes currently allocated to the user. */
	size_t				totalFree;		/**< Bytes currently free inside the allocator. */
	bool				initialized;	/**< True once init has completed. */



	t_hmallocStats		statsSubproc;	/**< Accumulateur global (mmap, arènes, erreurs). */
	t_hmallocStats		statsHeapMain;	/**< Stats du heap principal. */

} t_hmallocGlobal;

/**
 * @brief The single global instance dedicated to allocation.
 */
extern t_hmallocGlobal	g_hmalloc;

/* ----- Alignment helpers ----- */

/**
 * @brief Check whether a value is a power of two.
 * @param x Value to check.
 * @return true if `x` is a power of two (and not zero).
 */
static inline bool	hmallocIsPow2(uintptr_t x)
{
	return (x != 0 && (x & (x - 1)) == 0);
}

/**
 * @brief Align a value up to the next multiple of `a`.
 * @param sz Value to align.
 * @param a  Alignment; must be a power of two.
 * @return Aligned value.
 */
static inline uintptr_t	hmallocAlignUp(uintptr_t sz, size_t a)
{
	const uintptr_t	m = (uintptr_t)a - 1;

	return ((sz + m) & ~m);
}

/**
 * @brief Align a value down to the previous multiple of `a`.
 * @param sz Value to align.
 * @param a  Alignment; must be a power of two.
 * @return Aligned value.
 */
static inline uintptr_t	hmallocAlignDown(uintptr_t sz, size_t a)
{
	const uintptr_t	m = (uintptr_t)a - 1;

	return (sz & ~m);
}

/**
 * @brief Align a pointer up to the next multiple of `a`.
 * @param p Pointer to align.
 * @param a Alignment; must be a power of two.
 * @return Aligned pointer.
 */
static inline void	*hmallocAlignUpPtr(const void *p, size_t a)
{
	return ((void *)hmallocAlignUp((uintptr_t)p, a));
}

/**
 * @brief Check whether a pointer is aligned to `a`.
 * @param p Pointer to check.
 * @param a Alignment; must be a power of two.
 * @return true if `p` is aligned.
 */
static inline bool	hmallocIsAligned(const void *p, size_t a)
{
	return (((uintptr_t)p % a) == 0);
}

/* ----- Overflow helpers ----- */

/**
 * @brief Check whether `a * b` overflows a size_t.
 * @param a   First operand.
 * @param b   Second operand.
 * @param out Output: the product when no overflow occurs.
 * @return true on overflow.
 */
static inline bool	hmallocMulOverflow(size_t a, size_t b, size_t *out)
{
	return (__builtin_mul_overflow(a, b, out));
}

/**
 * @brief Check whether `count * size` overflows a size_t.
 * @param count Element count.
 * @param size  Element size.
 * @param total Output: the product when no overflow occurs.
 * @return true on overflow.
 */
static inline bool	hmallocCountSizeOverflow(size_t count, size_t size,
											 size_t *total)
{
	if (count == 1) {
		*total = size;
		return (false);
	}
	if (hmLikely(!hmallocMulOverflow(count, size, total)))
		return (false);
	*total = SIZE_MAX;
	return (true);
}

/* ----- Min / max / clamp ----- */

/**
 * @brief Return the smaller of two size_t values.
 * @param a First value.
 * @param b Second value.
 * @return Minimum.
 */
static inline size_t	hmallocMin(size_t a, size_t b)
{
	return (a <= b ? a : b);
}

/**
 * @brief Return the larger of two size_t values.
 * @param a First value.
 * @param b Second value.
 * @return Maximum.
 */
static inline size_t	hmallocMax(size_t a, size_t b)
{
	return (a >= b ? a : b);
}

/**
 * @brief Clamp a size_t to a range.
 * @param s  Value to clamp.
 * @param lo Lower bound.
 * @param hi Upper bound.
 * @return Clamped value.
 */
static inline size_t	hmallocClamp(size_t s, size_t lo, size_t hi)
{
	return (s < lo ? lo : (s > hi ? hi : s));
}

/* ----- Encoded pointer helpers ----- */

/**
 * @brief Decode a value produced by hmallocPtrEncode.
 * @param null Value to return for a null encoded pointer.
 * @param x    Encoded pointer.
 * @param keys Two obfuscation keys (per-page).
 * @return Decoded pointer.
 */
static inline void	*hmallocPtrDecode(const void *null, t_hmEncoded x,
									  const uintptr_t *keys)
{
	const uintptr_t	k1 = keys[0];
	const uintptr_t	k2 = keys[1];
	void			*p;

	p = (void *)(hmallocRotr(x - k1, k1) ^ k2);
	return (p == null ? NULL : p);
}

/**
 * @brief Encode a pointer for storage in a free-list.
 * @param null Value to encode for a null pointer.
 * @param p    Pointer to encode.
 * @param keys Two obfuscation keys (per-page).
 * @return Encoded value.
 */
static inline t_hmEncoded	hmallocPtrEncode(const void *null, const void *p,
											 const uintptr_t *keys)
{
	const uintptr_t	k1 = keys[0];
	const uintptr_t	k2 = keys[1];
	const uintptr_t	x = (uintptr_t)(p == NULL ? null : p);

	return ((t_hmEncoded)(hmallocRotl(x ^ k2, k1) + k1));
}

/* ----- Page helpers ----- */

/**
 * @brief Return the block size served by a page.
 * @param p Page to inspect.
 * @return Block size in bytes.
 */
static inline size_t	hmallocPageBlockSize(const t_hmallocPage *p)
{
	return (p->blockSize);
}

/**
 * @brief Return the address of the first block of a page.
 * @param p Page to inspect.
 * @return Pointer to the first block.
 */
static inline uint8_t	*hmallocPageStart(const t_hmallocPage *p)
{
	return ((uint8_t *)p + p->pageOffset);
}

/**
 * @brief Return whether the page is currently owned by some thread.
 * @param p Page to inspect.
 * @return true if the low bit of `xthreadFree` is set.
 */
static inline bool	hmallocPageIsOwned(const t_hmallocPage *p)
{
	const uintptr_t	v = atomic_load_explicit(&p->xthreadFree,
											  memory_order_relaxed);

	return ((v & 1) != 0);
}

/**
 * @brief Return the head of the cross-thread free-list of a page.
 * @param p Page to inspect.
 * @return Head of the remote free-list, or NULL.
 */
static inline t_hmallocBlock	*hmallocPageThreadFree(const t_hmallocPage *p)
{
	const uintptr_t	v = atomic_load_explicit(&p->xthreadFree,
											  memory_order_relaxed);

	return ((t_hmallocBlock *)(v & ~(uintptr_t)1));
}

/* ----- Block helpers ----- */
/* ------------------------------------------------------------------ */
/*  Block helpers                                                      */
/* ------------------------------------------------------------------ */

#ifndef HMALLOC_ENCODE_FREELIST
# define HMALLOC_ENCODE_FREELIST 0   /* désactivé par défaut pour la perf */
#endif

static inline t_hmallocBlock *
hmallocBlockNext(const t_hmallocPage *page, const t_hmallocBlock *b)
{
#if HMALLOC_ENCODE_FREELIST
    return hmallocPtrDecode(page, b->next, page->keys);
#else
    (void)page;
    return (t_hmallocBlock *)(uintptr_t)b->next;
#endif
}

static inline void
hmallocBlockSetNext(const t_hmallocPage *page, t_hmallocBlock *b,
                    const t_hmallocBlock *next)
{
#if HMALLOC_ENCODE_FREELIST
    b->next = hmallocPtrEncode(page, next, page->keys);
#else
    (void)page;
    b->next = (t_hmEncoded)(uintptr_t)next;
#endif
}


/* ----- Message helpers ----- */

/**
 * @brief Format a message into a caller buffer.
 *
 * Supports `%s %c %d %u %zu %lu %x %lX %p %%`. Never calls
 * the libc snprintf, which may itself call malloc.
 *
 * @param buf Output buffer.
 * @param cap Capacity of `buf`.
 * @param fmt Format string.
 * @param ap  Variadic argument list.
 * @return Number of bytes written, excluding the NUL terminator.
 */
size_t	hmallocFmt(char *buf, size_t cap, const char *fmt, va_list ap);

/**
 * @brief Convert an unsigned integer to a string.
 *
 * @param buf   Output buffer.
 * @param v     Value to convert.
 * @param base  Base in [2, 36].
 * @param upper True to use uppercase digits.
 * @return Number of bytes written, excluding the NUL.
 */
size_t	hmallocUtoa(char *buf, size_t cap, uint64_t v,
				   unsigned base, bool upper);

/* ----- Internal prototypes ----- */

void	_hmallocOptionsInit(void);
void	_hmallocOptionsPostInit(void);
void	_hmallocErrorMessage(int err, const char *fmt, ...);
void	_hmallocWarningMessage(const char *fmt, ...);
void	_hmallocVerboseMessage(const char *fmt, ...);

void	_hmallocOsInit(void);
size_t	_hmallocOsPageSize(void);

void	*_hmallocMetaZalloc(t_hmallocSubproc *subproc, size_t size,
							t_hmallocMemId *memid);
void	_hmallocMetaFree(t_hmallocSubproc *subproc, void *p,
						 t_hmallocMemId memid);

void	_hmallocAutoProcessInit(void);
void	_hmallocAutoProcessDone(void);
bool	_hmallocPreloading(void);

t_hmallocTheap	*_hmallocThreadInit(void);
void			_hmallocThreadDone(t_hmallocTheap *theap);

void		_hmallocStatsInit(void);
void		_hmallocStatsMergeInto(struct s_hmallocStats *to,
								  struct s_hmallocStats *from);
t_hmMsecs	_hmallocClockNow(void);

void	_hmallocRandomInit(struct s_hmallocTld *tld);
size_t	_hmallocRandomNext(struct s_hmallocTld *tld);


#endif /* HMALLOC_INTERNAL_H */
