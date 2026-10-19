/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file hmalloc.h
 * @brief Public API of the hmalloc allocator.
 * @Created: 2026/10/05 15:16:12 by Moutig
 * @Updated: 2026/10/16 08:03:55 by Moutig
 *
 * hmalloc is the dynamic memory allocator shipped with
 * hajlibc. It backs the standard `malloc`, `free`, `realloc`
 * and `calloc` symbols, and exposes additional entry points
 * (`h*` prefix) that give the caller control over alignment,
 * usable size, heaps, statistics and profiling.
 *
 * The allocator is built on top of `mmap`/`munmap` only: it
 * never calls the host `malloc`, so it can be used to
 * implement it. Its fast path is lock-free and syscall-free.
 *
 * Conventions
 * -----------
 *   - All `h*` functions are safe to call from any thread.
 *   - `NULL` is a valid input for every "free" style function
 *     and is a no-op.
 *   - On failure, `h*` functions return `NULL` and set `errno`
 *     to `ENOMEM` (allocation) or `EINVAL` (invalid argument).
 *   - The allocator never returns a partially initialised
 *     block. On success, the returned pointer is 16-byte
 *     aligned (or stricter when requested via
 *     `hmallocAligned` / `posix_memalign`).
 *
 * This header is intended to be included from user code that
 * wants the extended API. Programs that only need the POSIX
 * allocator should include `<stdlib.h>` instead.
 */

#ifndef HMALLOC_H
# define HMALLOC_H

# include <stddef.h>
# include <stdint.h>
# include <stdbool.h>

# ifdef __cplusplus
extern "C" {
# endif

/* ----- Version ----- */

/**
 * @brief hmalloc version, encoded as major*10000 + minor*100 + patch.
 */
# define HMALLOC_VERSION	10000

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

# define HMALLOC_DEBUG_LEVEL	4
# ifndef HMALLOC_DEBUG_LEVEL
#  define HMALLOC_DEBUG_LEVEL	0
# endif


/* ----- Core API ----- */

/**
 * @brief Allocate an uninitialised block of memory.
 *
 * Allocates `size` bytes of memory. The returned pointer is
 * suitably aligned for any object whose size is at most the
 * platform's maximum fundamental alignment (16 bytes on
 * x86_64 and aarch64).
 *
 * A request of size 0 returns a unique, non-NULL pointer that
 * can be passed to `hfree`. The content of the block is
 * undefined.
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to the allocated block, or NULL on failure
 *         (with errno set to ENOMEM).
 */
_hmDeclNodiscard _hmDeclExport _hmDeclRestrict
void	*hmalloc(size_t size)
		_hmAttrNoexcept _hmAttrMalloc _hmAttrAllocSize(1);

/**
 * @brief Allocate and zero-initialise an array.
 *
 * Allocates `count * size` bytes. If the multiplication
 * overflows, the request fails with errno = ENOMEM and NULL is
 * returned. The returned block is guaranteed to be all zero
 * bytes.
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
 * Changes the size of the block pointed to by `p` to `newsize`
 * bytes. The content of the block is preserved up to the
 * minimum of the old and new sizes.
 *
 * Behaviour matches POSIX `realloc`:
 *   - if `p` is NULL, equivalent to `hmalloc(newsize)`.
 *   - if `newsize` is 0, a new block of size 0 is returned and
 *     `p` is freed. The result is never NULL unless allocation
 *     of the zero-sized block fails.
 *   - if allocation fails, `p` is left untouched and NULL is
 *     returned with errno set.
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
 * Frees the block pointed to by `p`, which must have been
 * returned by `hmalloc`, `hcalloc`, `hrealloc` or any of the
 * aligned variants. If `p` is NULL, this function is a no-op.
 *
 * Passing an invalid pointer results in undefined behaviour,
 * except when `HMALLOC_CHECK` is enabled at runtime, in which
 * case the allocator reports the error through the registered
 * error callback and returns without corrupting its state.
 *
 * @param p Block to free, or NULL.
 */
_hmDeclExport
void	hfree(void *p)
		_hmAttrNoexcept;

/**
 * @brief Allocate a block with a specific alignment.
 *
 * Allocates `size` bytes aligned to `alignment`. The alignment
 * must be a power of two; values below 16 are rounded up to
 * the platform's natural alignment.
 *
 * For alignments larger than the page size, the allocator uses
 * a dedicated mapping. The returned block must be released
 * with `hfree`.
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

/**
 * @brief Allocate and zero-initialise a block.
 *
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
 * Allocates `count * size` bytes. If the multiplication
 * overflows, the request fails with errno = ENOMEM. Unlike
 * `hcalloc`, the content of the block is undefined.
 *
 * @param count Number of elements.
 * @param size  Size of each element in bytes.
 * @return Pointer to the block, or NULL on failure.
 */
_hmDeclNodiscard _hmDeclExport _hmDeclRestrict
void	*hmallocn(size_t count, size_t size)
		_hmAttrNoexcept _hmAttrMalloc _hmAttrAllocSize2(1,2);

/**
 * @brief Return the usable size of a block.
 *
 * The usable size is the number of bytes that can be safely
 * accessed starting from the returned pointer, and is always
 * greater than or equal to the size that was requested at
 * allocation time.
 *
 * If `p` is NULL, the function returns 0.
 *
 * @param p Previously allocated block, or NULL.
 * @return Usable size in bytes.
 */
_hmDeclNodiscard _hmDeclExport
size_t	husableSize(const void *p)
		_hmAttrNoexcept;

/**
 * @brief Round a size up to the next allocatable bucket.
 *
 * Returns the smallest size that is greater than or equal to
 * `size` and that the allocator can serve without wasting a
 * full bucket. This is the same value that `husableSize` will
 * report after a successful allocation of `size` bytes.
 *
 * @param size Requested size in bytes.
 * @return Usable size for that request.
 */
_hmDeclNodiscard _hmDeclExport
size_t	hgoodSize(size_t size)
		_hmAttrNoexcept;

/**
 * @brief Try to enlarge a block in place.
 *
 * If the block can grow to `newsize` without moving, the
 * function updates its usable size and returns `p`. Otherwise,
 * it returns NULL and the block is left unchanged. The caller
 * is expected to fall back to `hrealloc` on NULL.
 *
 * @param p       Previously allocated block.
 * @param newsize Requested new size in bytes.
 * @return `p` on success, NULL if the block cannot grow in
 *         place.
 */
_hmDeclExport
void	*hexpand(void *p, size_t newsize)
		_hmAttrNoexcept;

/**
 * @brief Free a block whose size is known.
 *
 * Equivalent to `hfree(p)`, but the caller provides the size
 * that was originally requested. When `HMALLOC_CHECK` is
 * enabled, the allocator verifies that the size is consistent
 * with the block metadata and reports a mismatch through the
 * error callback.
 *
 * @param p    Block to free, or NULL.
 * @param size Size originally requested.
 */
_hmDeclExport
void	hfreeSize(void *p, size_t size)
		_hmAttrNoexcept;


/* ----- Heap API ----- */

/**
 * @brief Opaque handle to an allocator heap.
 *
 * A heap is an independent collection of pages. The allocator
 * ships with a default heap, but the caller may create
 * additional ones to isolate unrelated workloads (for example
 * one heap per subsystem), and free all of their memory at
 * once by destroying the heap.
 */
typedef struct s_hmallocHeap	t_hmallocHeap;

/**
 * @brief Create a new heap.
 *
 * The new heap is empty. Allocating from it does not interfere
 * with the default heap, and the memory it owns can be
 * released in a single call to `hmHeapDelete` or
 * `hmHeapDestroy`.
 *
 * @return Pointer to the new heap, or NULL on failure
 *         (errno = ENOMEM).
 */
_hmDeclNodiscard _hmDeclExport
t_hmallocHeap	*hmHeapNew(void);

/**
 * @brief Delete a heap, keeping live blocks valid.
 *
 * Marks the heap as deleted and transfers ownership of every
 * page still in use to the default heap, so that the caller's
 * live pointers remain valid. Empty pages are returned to the
 * system immediately. After this call, the pointer `heap` is
 * invalid and must not be used.
 *
 * Calling this function on the default heap is a no-op.
 *
 * @param heap Heap to delete, or NULL.
 */
_hmDeclExport
void	hmHeapDelete(t_hmallocHeap *heap);

/**
 * @brief Destroy a heap, releasing every page.
 *
 * Releases every page owned by the heap, including pages whose
 * blocks are still in use. This is intended for teardown
 * scenarios (for example at the end of a subsystem) where the
 * caller knows that no live pointer refers to the heap.
 *
 * Calling this function on the default heap is a no-op.
 *
 * @param heap Heap to destroy, or NULL.
 */
_hmDeclExport
void	hmHeapDestroy(t_hmallocHeap *heap);

/**
 * @brief Run garbage collection on a heap.
 *
 * Scans the heap's pages, collects deferred frees, and returns
 * empty pages to the system. If `force` is true, the function
 * also releases pages that would normally be kept for reuse.
 *
 * @param heap  Heap to collect, or NULL for the default heap.
 * @param force If true, release pages kept for reuse.
 */
_hmDeclExport
void	hmHeapCollect(t_hmallocHeap *heap, bool force);

/**
 * @brief Allocate from a specific heap.
 *
 * Behaves like `hmalloc`, but the block is owned by `heap`.
 * Freeing the block with `hfree` is still correct; the
 * allocator finds the owning heap from the block itself.
 *
 * @param heap Heap to allocate from, or NULL for the default
 *             heap.
 * @param size Size in bytes.
 * @return Pointer to the allocated block, or NULL on failure.
 */
_hmDeclNodiscard _hmDeclExport _hmDeclRestrict
void	*hmHeapMalloc(t_hmallocHeap *heap, size_t size)
		_hmAttrNoexcept _hmAttrMalloc _hmAttrAllocSize(2);


/* ----- Global helpers ----- */

/**
 * @brief Run garbage collection on the default heap.
 *
 * Equivalent to `hmHeapCollect(NULL, force)`.
 *
 * @param force If true, release pages kept for reuse.
 */
_hmDeclExport
void	hmCollect(bool force)
		_hmAttrNoexcept;

/**
 * @brief Return the allocator version.
 *
 * The value is encoded as `major*10000 + minor*100 + patch`,
 * matching the `HMALLOC_VERSION` macro.
 *
 * @return Version number.
 */
_hmDeclExport
int	hmVersion(void);

/**
 * @brief Check whether the allocator owns the standard symbols.
 *
 * Returns true when the process is using hmalloc as its
 * `malloc`/`free`/`realloc` implementation, and false
 * otherwise. Useful in `LD_PRELOAD` scenarios to detect
 * whether the override is active.
 *
 * @return true if hmalloc is the active allocator.
 */
_hmDeclExport
bool	hmIsRedirected(void)
		_hmAttrNoexcept;

/**
 * @brief Query process-wide memory information.
 *
 * Fills the provided pointers with the process statistics
 * exposed by the operating system. Any pointer may be NULL, in
 * which case the corresponding value is skipped.
 *
 * @param elapsedMsecs   Elapsed wall time since process start.
 * @param userMsecs      User CPU time.
 * @param systemMsecs    System CPU time.
 * @param currentRss     Current resident set size in bytes.
 * @param peakRss        Peak resident set size in bytes.
 * @param currentCommit  Current committed memory in bytes.
 * @param peakCommit     Peak committed memory in bytes.
 * @param pageFaults     Number of page faults since start.
 */
_hmDeclExport
void	hmProcessInfo(size_t *elapsedMsecs, size_t *userMsecs,
					  size_t *systemMsecs, size_t *currentRss,
					  size_t *peakRss, size_t *currentCommit,
					  size_t *peakCommit, size_t *pageFaults)
		_hmAttrNoexcept;


/* ----- Callbacks ----- */

/**
 * @brief Callback used to emit a textual message.
 *
 * @param msg NUL-terminated message.
 * @param arg Caller-provided argument.
 */
typedef void (_hmCdecl t_hmOutputFun)(const char *msg, void *arg);

/**
 * @brief Callback used to report an error.
 *
 * @param err Positive errno-style error code.
 * @param arg Caller-provided argument.
 */
typedef void (_hmCdecl t_hmErrorFun)(int err, void *arg);

/**
 * @brief Callback invoked periodically to drive deferred frees.
 *
 * @param force     Non-zero when the allocator requests an
 *                  immediate flush.
 * @param heartbeat Monotonic counter incremented on every
 *                  allocator tick.
 * @param arg       Caller-provided argument.
 */
typedef void (_hmCdecl t_hmDeferredFreeFun)(bool force,
											unsigned long long heartbeat,
											void *arg);

/**
 * @brief Register the output callback.
 *
 * Passing NULL restores the default callback, which writes to
 * file descriptor 2.
 *
 * @param out Callback, or NULL for the default.
 * @param arg Argument passed to the callback.
 */
_hmDeclExport
void	hmRegisterOutput(t_hmOutputFun *out, void *arg)
		_hmAttrNoexcept;

/**
 * @brief Register the error callback.
 *
 * Passing NULL restores the default callback, which writes to
 * file descriptor 2 when the SHOW_ERRORS option is enabled.
 *
 * @param fun Callback, or NULL for the default.
 * @param arg Argument passed to the callback.
 */
_hmDeclExport
void	hmRegisterError(t_hmErrorFun *fun, void *arg)
		_hmAttrNoexcept;

/**
 * @brief Register the deferred-free callback.
 *
 * The callback is invoked periodically by the allocator to let
 * the application drive its own deferred-free logic.
 *
 * @param fn  Callback, or NULL to clear.
 * @param arg Argument passed to the callback.
 */
_hmDeclExport
void	hmRegisterDeferredFree(t_hmDeferredFreeFun *fn, void *arg)
		_hmAttrNoexcept;


/* ----- 42 project ----- */

/**
 * @brief Print the state of all allocated blocks.
 *
 * Prints one line per allocated block, grouped by category
 * (TINY, SMALL, LARGE) and sorted by increasing address. The
 * format is the one required by the 42 malloc subject:
 *
 *     TINY : 0xA0000
 *     0xA0020 - 0xA004A : 42 bytes
 *     SMALL : 0xAD000
 *     0xAD020 - 0xADEAD : 3725 bytes
 *     LARGE : 0xB0000
 *     0xB0020 - 0xBBEEF : 48847 bytes
 *     Total : 52698 bytes
 *
 * The output goes to file descriptor 1.
 */
_hmDeclExport
void	show_alloc_mem(void);

/**
 * @brief Print the state of all allocated blocks, with details.
 *
 * Same as `show_alloc_mem`, but also dumps the first bytes of
 * each block and, when tracing is enabled, the history of the
 * last allocations and frees. The exact output is controlled
 * by the `VERBOSE` and `TRACE` options.
 */
_hmDeclExport
void	show_alloc_mem_ex(void);


/* ----- Options ----- */

/**
 * @brief Runtime options of the allocator.
 *
 * Options can be set at process start through environment
 * variables (`HMALLOC_<NAME>`) or at runtime with
 * `hmOptionSet`.
 */
typedef enum e_hmallocOption {
	HMALLOC_OPT_SHOW_ERRORS = 0,	/**< Emit error messages. */
	HMALLOC_OPT_SHOW_STATS,			/**< Print stats at exit. */
	HMALLOC_OPT_VERBOSE,			/**< Verbose output. */
	HMALLOC_OPT_PURGE_DELAY,		/**< Delay before returning slices. */
	HMALLOC_OPT_ARENA_RESERVE,		/**< Arena reservation size (KiB). */
	HMALLOC_OPT_PAGE_FULL_RETAIN,	/**< Full pages kept for reuse. */
	HMALLOC_OPT_GUARDED_SAMPLE_RATE,/**< Guarded-allocation sample rate. */
	HMALLOC_OPT_PERTURB,			/**< Fill new and freed blocks. */
	HMALLOC_OPT_CHECK,				/**< Enable runtime checks. */
	HMALLOC_OPT_TRACE,				/**< Record an allocation trace. */
	HMALLOC_OPT_LAST				/**< Number of options. */
} t_hmallocOption;

/**
 * @brief Set an option.
 *
 * Out-of-range options are silently ignored, so this function
 * is safe to call from any context.
 *
 * @param opt   Option to set.
 * @param value New value.
 */
_hmDeclExport
void	hmOptionSet(t_hmallocOption opt, long value);

/**
 * @brief Get an option.
 *
 * Out-of-range options return 0.
 *
 * @param opt Option to query.
 * @return Current value of the option.
 */
_hmDeclExport
long	hmOptionGet(t_hmallocOption opt);


/* ----- Statistics ----- */

/**
 * @brief Version tag of the statistics structure.
 */
# define HMALLOC_STAT_VERSION	1

/**
 * @brief A counter that tracks total, peak and current values.
 *
 * Used for byte counts and page counts where the peak and the
 * current value matter.
 */
typedef struct s_hmallocStatCount {
	int64_t	total;		/**< Cumulative value since start. */
	int64_t	peak;		/**< Maximum value ever reached. */
	int64_t	current;	/**< Current value. */
} t_hmallocStatCount;

/**
 * @brief A monotonic counter.
 *
 * Used for values that only ever increase (number of calls,
 * number of bytes ever requested, etc.).
 */
typedef struct s_hmallocStatCounter {
	int64_t	total;		/**< Cumulative value since start. */
} t_hmallocStatCounter;

/**
 * @brief Number of bins in the statistics arrays.
 *
 * Matches `HMALLOC_BIN_HUGE` from the internal headers.
 */
# define HMALLOC_STAT_BIN_HUGE	73U

/**
 * @brief Snapshot of the allocator statistics.
 *
 * The `size` and `version` fields must be filled by the
 * caller before calling `hmStatsGet`. The
 * `hmallocStatsInit` helper does this automatically.
 */
/* ---------- Stats ---------- */
/**
 * @brief Number of block-size bins per thread-local heap.
 *
 * Bins 1..HMALLOC_BIN_HUGE-1 are for regular sizes, bin
 * HMALLOC_BIN_HUGE is the "huge" sentinel, bin
 * HMALLOC_BIN_HUGE + 1 is the FULL queue.
 */
# define HMALLOC_BIN_HUGE		73
# define HMALLOC_BIN_FULL		(HMALLOC_BIN_HUGE + 1)
# define HMALLOC_BIN_COUNT		(HMALLOC_BIN_FULL + 1)
/*
 * FIX PERF : on étend le fast path à 4 KiB au lieu de 1 KiB.
 * Cela couvre 1KB, 2KB, 4KB, ce qui est le gros des charges
 * réalistes (string_workload, realloc_growth, mixed_wide).
 *
 * Coût : pagesFreeDirect passe de 129 à 513 entrées par theap
 * (soit ~4 KiB par thread). Acceptable.
 */
#ifndef HMALLOC_TINY_MAX_SIZE
# define HMALLOC_TINY_MAX_SIZE	4096
#endif

/* Index direct : padded/8, arrondi au supérieur. */
#define HMALLOC_PAGES_DIRECT	((HMALLOC_TINY_MAX_SIZE >> 3) + 2)
# ifndef HMALLOC_FREE_PAGES_PER_BIN
#  define HMALLOC_FREE_PAGES_PER_BIN   3u
# endif
#define HMALLOC_STATS_VERSION	1

typedef struct s_hmallocStats {
	size_t			size;			/* = sizeof(t_hmallocStats) */
	int				version;		/* = HMALLOC_STATS_VERSION */

	/* Totaux utilisateur */
	size_t			total;			/* octets utilisateur actuellement alloués */
	size_t			peak;			/* pic de `total` */
	size_t			current;		/* octets réservés à l'allocateur */

	/* Pages / arènes */
	size_t			pages;
	size_t			reserved;
	size_t			committed;
	size_t			mmapCalls;
	size_t			arenaCount;

	/* Compteurs malloc */
	size_t			mallocNormalCount;
	size_t			mallocHugeCount;
	size_t			mallocRequested;
	size_t			errors;

	/* Par bin */
	size_t			mallocBins[HMALLOC_BIN_COUNT];
	size_t			pageBins[HMALLOC_BIN_COUNT];
} t_hmallocStats;

bool	hmStatsGet(t_hmallocStats *stats);
char	*hmStatsGetJson(size_t bufSize, char *buf);
void	hmStatsPrintOut(t_hmOutputFun *out, void *arg);
void	hmStatsPrint(void *out);
bool	hmHeapStatsGet(t_hmallocHeap *heap, t_hmallocStats *stats);
char	*hmHeapStatsGetJson(t_hmallocHeap *heap, size_t bufSize, char *buf);
void	hmHeapStatsPrintOut(t_hmallocHeap *heap,
							 t_hmOutputFun *out, void *arg);
void	hmStatsReset(void);
void	hmStatsMerge(void);
void	hmProcessInfo(size_t *elapsedMsecs, size_t *userMsecs,
					   size_t *systemMsecs, size_t *currentRss,
					   size_t *peakRss, size_t *currentCommit,
					   size_t *peakCommit, size_t *pageFaults);
/**
 * @brief Initialise the header of a statistics structure.
 *
 * @param s Statistics structure to initialise.
 */
static inline void hmallocStatsHeaderInit(t_hmallocStats *s)
{
	s->size = sizeof(*s);
	s->version = HMALLOC_STAT_VERSION;
}

/**
 * @brief Zero-initialise a statistics structure.
 *
 * @param s Statistics structure to initialise.
 */
static inline void hmallocStatsInit(t_hmallocStats *s)
{
	for (size_t i = 0; i < sizeof(*s) / sizeof(unsigned char); i++)
		((unsigned char *)s)[i] = 0;
	hmallocStatsHeaderInit(s);
}

/**
 * @brief Convenience macro to declare and initialise a snapshot.
 *
 * @param name Variable name to declare.
 */
# define HMALLOC_STATS_DECL(name) \
	t_hmallocStats name; \
	hmallocStatsInit(&name);

/**
 * @brief Fill a statistics snapshot with the current values.
 *
 * @param stats Output structure. The `size` and `version`
 *              fields must already be filled.
 * @return true on success, false if the structure is
 *         incompatible (wrong size or version).
 */
_hmDeclNodiscard _hmDeclExport
bool	hmStatsGet(t_hmallocStats *stats)
		_hmAttrNoexcept;

/**
 * @brief Serialise the current statistics as JSON.
 *
 * @param bufSize Size of the output buffer.
 * @param buf     Output buffer.
 * @return Pointer to `buf` on success, or NULL if the buffer
 *         is too small.
 */
_hmDeclNodiscard _hmDeclExport
char	*hmStatsGetJson(size_t bufSize, char *buf)
		_hmAttrNoexcept;

/**
 * @brief Send the current statistics to a callback.
 *
 * @param out Callback invoked for each line.
 * @param arg Argument passed to the callback.
 */
_hmDeclExport
void	hmStatsPrintOut(t_hmOutputFun *out, void *arg)
		_hmAttrNoexcept;

/**
 * @brief Print the current statistics to a stream.
 *
 * In the standalone 42 build, `out` is interpreted as a file
 * descriptor encoded in a pointer (NULL means file descriptor
 * 2). In the hajlibc build, `out` is a `FILE *`.
 *
 * @param out Output stream, or NULL for the default.
 */
_hmDeclExport
void	hmStatsPrint(void *out)
		_hmAttrNoexcept;

/**
 * @brief Fill a snapshot with the statistics of a heap.
 *
 * @param heap  Heap to query, or NULL for the default heap.
 * @param stats Output structure, pre-initialised.
 * @return true on success, false on failure.
 */
_hmDeclNodiscard _hmDeclExport
bool	hmHeapStatsGet(t_hmallocHeap *heap, t_hmallocStats *stats)
		_hmAttrNoexcept;

/**
 * @brief Serialise the statistics of a heap as JSON.
 *
 * @param heap    Heap to query, or NULL for the default heap.
 * @param bufSize Size of the output buffer.
 * @param buf     Output buffer.
 * @return Pointer to `buf` on success, or NULL on failure.
 */
_hmDeclNodiscard _hmDeclExport
char	*hmHeapStatsGetJson(t_hmallocHeap *heap, size_t bufSize, char *buf)
		_hmAttrNoexcept;

/**
 * @brief Send the statistics of a heap to a callback.
 *
 * @param heap Heap to query, or NULL for the default heap.
 * @param out  Callback invoked for each line.
 * @param arg  Argument passed to the callback.
 */
_hmDeclExport
void	hmHeapStatsPrintOut(t_hmallocHeap *heap, t_hmOutputFun *out, void *arg)
		_hmAttrNoexcept;

/**
 * @brief Reset all statistics counters.
 *
 * The `current` values are preserved; only the `total` and
 * `peak` counters are cleared.
 */
_hmDeclExport
void	hmStatsReset(void)
		_hmAttrNoexcept;

/**
 * @brief Merge the statistics of the calling thread into the
 *        global counters.
 *
 * Called implicitly when a thread exits. May also be called
 * manually.
 */
_hmDeclExport
void	hmStatsMerge(void)
		_hmAttrNoexcept;


/* ----- Profiling ----- */

/**
 * @brief Maximum number of user data slots in a profile sample.
 */
# define HMALLOC_PROFILE_SAMPLE_DATA_MAX_SIZE	1024

/**
 * @brief Opaque profiler handle.
 */
typedef struct s_hmallocProfiler	t_hmallocProfiler;

/**
 * @brief Data attached to one profile sample.
 *
 * The `userData` array is sized by the profiler's
 * `sampleDataSize` field at runtime; the declaration here
 * shows one slot but the caller must allocate the full size.
 */
typedef struct s_hmallocProfilerSampleData {
	size_t	userDataSize;		/**< Size of the user data. */
	void	*userData[1];		/**< User data slots. */
} t_hmallocProfilerSampleData;

/**
 * @brief Callback invoked when an allocation is sampled.
 *
 * @param prof            Profiler instance.
 * @param data            Sample data buffer.
 * @param ptr             Allocated pointer.
 * @param requestedSize   Size requested by the caller.
 * @param sampleRate      Current sampling rate.
 * @param sinceLastSample Bytes allocated since the previous
 *                        sample.
 * @param heap            Heap that served the allocation.
 * @return New sampling rate, or 0 to disable further sampling.
 */
typedef size_t (_hmCdecl t_hmallocProfilerOnAllocFn)
	(t_hmallocProfiler *prof, t_hmallocProfilerSampleData *data,
	 void *ptr, size_t requestedSize, size_t sampleRate,
	 uint64_t sinceLastSample, const t_hmallocHeap *heap);

/**
 * @brief Callback invoked when a reallocation is sampled.
 *
 * @param prof    Profiler instance.
 * @param data    Sample data buffer.
 * @param ptr     New pointer.
 * @param oldSize Size before the reallocation.
 * @param heap    Heap that served the reallocation.
 * @return New sampling rate.
 */
typedef size_t (_hmCdecl t_hmallocProfilerOnReallocFn)
	(t_hmallocProfiler *prof, t_hmallocProfilerSampleData *data,
	 void *ptr, size_t oldSize, const t_hmallocHeap *heap);

/**
 * @brief Callback invoked when a free is sampled.
 *
 * @param prof Profiler instance.
 * @param data Sample data buffer.
 * @param ptr  Freed pointer.
 * @param heap Heap that owned the block.
 */
typedef void (_hmCdecl t_hmallocProfilerOnFreeFn)
	(t_hmallocProfiler *prof, t_hmallocProfilerSampleData *data,
	 void *ptr, const t_hmallocHeap *heap);

/**
 * @brief Profiler descriptor.
 *
 * The caller fills this structure and passes it to one of the
 * `hmProfilerStart` / `hmHeapProfile` entry points.
 */
typedef struct s_hmallocProfiler {
	size_t			initialSampleRate;	/* intervalle initial (octets) */
	size_t			sampleRate;			/* taux courant */
	size_t			sampleCountdown;	/* countdown avant prochain sample */
	size_t			sampleCount;		/* samples collectés depuis Start */
	size_t			totalSamples;		/* total cumulé */

	void			*arg;				/* argument utilisateur */
	void			(*onSample)(void *arg, const void *block,
									size_t requested, size_t usable);
	void			(*onStop)(void *arg);

	struct s_hmallocHeap	*heap;		/* heap attaché (NULL = global) */
	bool			active;				/* vrai entre Start et Stop */
} t_hmallocProfiler;
/**
 * @brief Attach a profiler to a heap.
 *
 * @param heap Heap to profile, or NULL for the default heap.
 * @param prof Profiler descriptor.
 * @return true on success, false on failure.
 */
_hmDeclExport
bool	hmHeapProfile(t_hmallocHeap *heap, t_hmallocProfiler *prof);

/**
 * @brief Detach the profiler from a heap.
 *
 * @param heap Heap to unprofile, or NULL for the default heap.
 */
_hmDeclExport
void	hmHeapProfileDisable(t_hmallocHeap *heap);

/**
 * @brief Attach a profiler to the default heap.
 *
 * Equivalent to `hmHeapProfile(NULL, prof)`.
 *
 * @param prof Profiler descriptor.
 * @return true on success, false on failure.
 */
_hmDeclExport
bool	hmProfile(t_hmallocProfiler *prof);

/**
 * @brief Start a profiler.
 *
 * The profiler must already have been attached to a heap with
 * `hmProfile` or `hmHeapProfile`.
 *
 * @param prof Profiler descriptor.
 * @return true on success, false on failure.
 */
_hmDeclExport
bool	hmProfilerStart(t_hmallocProfiler *prof);

/**
 * @brief Stop a profiler.
 *
 * @param prof Profiler descriptor.
 * @return true on success, false on failure.
 */
_hmDeclExport
bool	hmProfilerStop(t_hmallocProfiler *prof);



# ifndef __HAJ_INLINE
#  if __HAJ_C_VERSION >= 199901L
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
