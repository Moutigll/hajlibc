/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file hmTypes.h
 * @brief Type definitions for the memory allocator.
 * @Created: 2026/10/05 15:26:28 by Moutig
 * @Updated: 2026/11/16 15:22:31 by Moutig
 *
 * This file contains the type definitions used by the hajlibc memory allocator.
 * It includes definitions for pages, blocks, arenas, heaps, and other
 * related structures. The types are designed to be efficient and to support
 * the allocator's features, such as cross-thread ownership tracking and
 * memory origin tracking.
 */

#ifndef HMALLOC_TYPES_H
# define HMALLOC_TYPES_H

# include "../../../../include/malloc.h"

# include "hmBits.h"
# include "hmBitmap.h"
# include "hmAtomic.h"

/* -----  Page classes ----- */

/*
 * Page kinds follow a size-proportional layout:
 *
 * 	TINY  : up to 10 KiB  -> 64 KiB page
 * 	SMALL : up to 64 KiB  -> 512 KiB page
 * 	LARGE : up to 512 KiB -> 4 MiB page
 *
 * This keeps at least six blocks per page while reducing the amount
 * stranded in large pages for medium-sized allocations.
 */
# define HMALLOC_TINY_MAX_OBJ_SIZE		((HMALLOC_TINY_PAGE_SIZE - (4u * 1024u)) / 6u)
# define HMALLOC_SMALL_MAX_OBJ_SIZE		(HMALLOC_MEDIUM_PAGE_SIZE / 8u)
/*
 * Direct page table: one entry per machine word of request size, up to
 * HMALLOC_SMALL_WSIZE_MAX words (1 KiB on 64-bit), plus room for the
 * padding words and the zero-size entry. Index is the word count of
 * (size + HMALLOC_PADDING_SIZE).
 * HMALLOC_PADDING_WSIZE must match HMALLOC_PADDING_SIZE (hmPrivate.h).
 */
# define HMALLOC_SMALL_WSIZE_MAX		128u
# define HMALLOC_SMALL_SIZE_MAX			(HMALLOC_SMALL_WSIZE_MAX * sizeof(size_t))
# if HMALLOC_CANARY
#  define HMALLOC_PADDING_WSIZE			1u
# else
#  define HMALLOC_PADDING_WSIZE			0u
# endif
# define HMALLOC_DIRECT_MAX_SIZE		HMALLOC_SMALL_SIZE_MAX
# define HMALLOC_DIRECT_PAGES			(HMALLOC_SMALL_WSIZE_MAX + HMALLOC_PADDING_WSIZE + 1u)

# define HMALLOC_TINY_PAGE_SIZE			(64u * 1024u)		/* N = 64 KiB */
# define HMALLOC_MEDIUM_PAGE_SIZE		(512u * 1024u)		/* M = 512 KiB */

# define HMALLOC_LARGE_PAGE_SIZE		(4u * 1024u * 1024u)	/* 4 MiB */
# define HMALLOC_LARGE_MAX_OBJ_SIZE		(HMALLOC_LARGE_PAGE_SIZE / 8u)
# define HMALLOC_LARGE_MAX_OBJ_WSIZE	(HMALLOC_LARGE_MAX_OBJ_SIZE / 8)

/* Historical SMALL bound, anything <= this goes to TINY or SMALL, above goes to LARGE. */
# define HMALLOC_SMALL_MAX_SIZE			HMALLOC_SMALL_MAX_OBJ_SIZE



/**
 * @brief Enable cross-thread ownership tracking.
 *
 * When enabled, the library will track which thread owns each page,
 * allowing for more efficient memory management across multiple threads.
 */
# ifndef HMALLOC_XTHREAD_OWNED
#  define HMALLOC_XTHREAD_OWNED		((uintptr_t)1)
# endif

/* ----- Block ----- */

/**
 * @brief Encoded pointer type for free-list.
 *
 * This type is used to store pointers in the free-list, with optional
 * XOR obfuscation for security. The low bits are used for flags.
 */
typedef uintptr_t	t_hmEncoded;

/**
 * @brief Thread ID type.
 *
 * This type is used to represent the ID of a thread.
 */
typedef size_t		t_hmallocThreadId;

/**
 * @struct s_hmallocBlock
 * @brief Fixed-size allocation unit inside a page.
 *
 * All blocks of a page share the same size (determined by the page's bin).
 * The single field `next` is meaningful only when the block is free:
 * it links the block into one of the page's free-lists. When allocated,
 * the block is raw user memory and `next` is overwritten.
 *
 * Lifecycle:
 *   created lazily (hmallocBlockSplit) -> allocated -> freed
 *   (page->free for owner, page->xthreadFree for remote)
 *   -> coalesced -> reused. Blocks are only released to the OS when the
 *   whole page is retired.
 *
 * If HMALLOC_CANARY > 0, a canary is written in the padding of each block to detect
 * buffer overflows. The canary is checked on free and realloc.
 */
typedef struct s_hmallocBlock {
	t_hmEncoded	next;
} t_hmallocBlock;

/* ----- Page ----- */

struct s_hmallocTheap;
struct s_hmallocHeap;
struct s_hmallocArena;

/**
 * @struct s_hmallocPage
 * @brief A page of memory containing fixed-size blocks.
 *
 * Each page is a contiguous memory region that contains a header
 * (this struct) followed by a number of fixed-size blocks. The
 * page is managed by a t_hmallocTheap and belongs to a t_hmallocArena.
 *
 * The page has three main states:
 * - Free: all blocks are free, the page can be reused or retired.
 * - Partial: some blocks are allocated, some are free.
 * - Full: all blocks are allocated, the page is in the full queue.
 *
 * The page also tracks cross-thread ownership and can be retired
 * when it is no longer needed.
 */
typedef struct s_hmallocPage {
	_Atomic t_hmallocThreadId	xthreadId;	/* Owner thread ID (0 = abandoned) */
	t_hmallocBlock				*free;		/* Free-list of blocks for the owner thread */
	t_hmallocBlock				*localFree;	/* Free-list of blocks freed by the owner thread */

	size_t		blockSize;		/* Size of each block in this page */
	size_t		pageOffset;		/* Offset from the start of the page to the first block */
	uint16_t	capacity;		/* Total number of blocks this page can hold */
	uint16_t	reserved;		/* Number of blocks reserved for this page (may be less than capacity) */
	uint16_t	used;			/* Number of blocks currently allocated */
# if HMALLOC_PROFILE
	uint16_t	allocCount;		/* Blocks handed out (wraps), for the sample countdown */
	uint16_t	lastAlloc;		/* allocCount at the last countdown update */
# endif
	uint8_t		retireExpire;	/* Number of cycles until this page can be retired */
	bool		freeIsZero;		/* True if the free-list is zeroed (no uninitialized memory) */
	/* Pad the hot fields to one cache line (48 bytes, +4 when profiling). */
	char		_pad_hot[64 - (5 * 8 + 3 * 2 + 2 + (HMALLOC_PROFILE ? 4 : 0))];

	/* Cache line 1 : xthreadFree isolated (avoids false sharing). */
	_Atomic uintptr_t	xthreadFree;			/* Free-list of blocks freed by other threads (cross-thread free) */
	uint64_t			_pad_xthread_free[7];

	/* Cache line 2 : cold metadata. */
	uintptr_t				keys[2];	/* Random keys for XOR obfuscation of free-list pointers */
	struct s_hmallocTheap	*theap;		/* Pointer to the owning thread heap (theap) */
	struct s_hmallocHeap	*heap;		/* Pointer to the owning heap (may be NULL if abandoned) */
	struct s_hmallocHeap	*abandonedHeap; /* Heap charged for this abandoned page */
	struct s_hmallocArena	*arena;		/* Pointer to the arena this page belongs to */
	struct s_hmallocPage	*next;		/* Next page in the bin's linked list */
	struct s_hmallocPage	*prev;		/* Previous page in the bin's linked list */
	void					*_reserved0;	/* Keeps sizeof(t_hmallocPage) a multiple of 64 */
	void					*osBase;	/* Start of the OS mapping of an OS allocated block */
	size_t					osBlockSize; /* Mapping size for individually mapped blocks */
	char					_pad_cold[40];
} t_hmallocPage;

_Static_assert(offsetof(t_hmallocPage, xthreadFree) % 64 == 0,
			   "xthreadFree must be on its own cache line");
_Static_assert(sizeof(t_hmallocPage) % 64 == 0,
			   "t_hmallocPage must be a multiple of 64");

/* Page flags encoded in the low bits of xthreadId. */

/** @brief Indicates that the page is in the queue of pages that are full (all blocks allocated). */
# define HMALLOC_PAGE_IN_FULL_QUEUE			((size_t)0x1)
/** @brief Indicates that the page has interior pointers (pointers to blocks inside the page). */
# define HMALLOC_PAGE_HAS_INTERIOR_POINTERS	((size_t)0x2)

# define HMALLOC_PAGE_FLAG_MASK				((size_t)0x3)

# define HMALLOC_THREADID_ABANDONED			((size_t)0)

/**
 * @struct s_hmallocPageQueue
 * @brief A queue of pages for a specific bin.
 *
 * Each bin has its own queue of pages, which can be in one of three states:
 * - Free: all blocks are free, the page can be reused or retired.
 * - Partial: some blocks are allocated, some are free.
 * - Full: all blocks are allocated, the page is in the full queue.
 *
 * The queue maintains pointers to the first and last pages, as well as
 * a count of the number of pages in the queue. The blockSize field is
 * used to identify the bin this queue belongs to.
 */
typedef struct s_hmallocPageQueue {
	t_hmallocPage	*first;
	t_hmallocPage	*last;
	size_t			count;
	size_t			blockSize;
	size_t			xcollectScore;	/* How often full pages here are revived by remote frees */
} t_hmallocPageQueue;

/* ----- Memory ID (origin tracking) ----- */

/**
 * @enum e_hmallocMemKind
 * @brief Identifies the origin of a memory region.
 *
 * This enum is used to track whether a memory region was allocated
 * by the operating system (OS) or by an arena managed by the allocator.
 */
typedef enum e_hmallocMemKind {
	HMALLOC_MEM_NONE,
	HMALLOC_MEM_OS,
	HMALLOC_MEM_ARENA,
} t_hmallocMemKind;


/**
 * @struct s_hmallocMemId
 * @brief Identifies a contiguous memory region and its origin.
 *
 * This struct is used to track the origin of a memory region, which can be
 * either from the OS (e.g., mmap) or from an arena managed by the allocator.
 * The `base` field points to the start of the memory region, `size` is the
 * size of the region in bytes, and `memkind` indicates whether it is from
 * the OS or an arena.
 */
typedef struct s_hmallocMemId {
	void				*base;	/* Base address of the memory region */
	size_t				size;	/* Size of the memory region in bytes */
	t_hmallocMemKind	memkind;/* Origin of the memory region (OS or arena) */
} t_hmallocMemId;

/* ----- Forward typedefs ----- */

typedef struct s_hmallocHeap		t_hmallocHeap;
typedef struct s_hmallocTheap		t_hmallocTheap;
typedef struct s_hmallocTld			t_hmallocTld;
typedef struct s_hmallocSubproc		t_hmallocSubproc;
typedef struct s_hmallocProfiler	t_hmallocProfiler;
typedef struct s_hmallocArena		t_hmallocArena;


/* ----- Theap (per-thread heap) ----- */

/**
 * @struct s_hmallocTheap
 * @brief Per-thread heap structure.
 *
 * Each thread has its own t_hmallocTheap, which manages the pages
 * and arenas for that thread. The theap maintains a list of pages
 * for each bin, as well as counters for full and retired pages.
 *
 * The theap also tracks the owning thread's ID, the associated heap,
 * and the associated subproc. It also contains statistics and profiling
 * information if enabled.
 */
struct s_hmallocTheap {
	t_hmallocPage	*pagesFreeDirect[HMALLOC_DIRECT_PAGES];	/* Direct free pages for small bins (O(1) allocation) */

	t_hmallocTld				*tld;						/* Pointer to the thread-local data (TLD) for this thread */
	_Atomic(t_hmallocHeap *)	heap;						/* Pointer to the owning heap (may be NULL if abandoned) */
	_Atomic(t_hmallocSubproc *)	subproc;					/* Pointer to the owning subproc (may be NULL if abandoned) */

	long				pageFullRetain;	/* Number of full pages to retain before retiring */
	unsigned long long	heartbeat;		/* Generic allocations since the last admin cycle */
	unsigned long long	genericCollectCount;	/* Generic allocations since the last full collect */

	uint16_t	pagesFullCount[HMALLOC_BIN_COUNT];	/* Count of full pages per bin (O(1) allocation) */

	uint16_t	retiredMinBin;	/* Minimum bin index with retired pages */
	uint16_t	retiredMaxBin;	/* Maximum bin index with retired pages */
	uint16_t	_pad_retired;	/* Padding to align the next field to a cache line boundary */
	t_hmallocPage	*hugeCached;	/* One empty huge page kept for reuse (retire delay) */

	t_hmallocTheap	*tnext;	/* Next theap in the thread's list of theaps */
	t_hmallocTheap	*tprev;	/* Previous theap in the thread's list of theaps */
	t_hmallocTheap	*hnext;	/* Next theap in the heap's list of theaps */
	t_hmallocTheap	*hprev;	/* Previous theap in the heap's list of theaps */

	t_hmallocPageQueue	pages[HMALLOC_BIN_COUNT];	/* Page queues for each bin (free, partial, full) */
	t_hmallocMemId		memid;						/* Memory ID for the theap's memory region */

# if HMALLOC_STATS
	t_hmallocStats		statsStorage;	/* Storage for statistics if enabled */
	t_hmallocStats		*stats;			/* Pointer to statistics (may point to statsStorage or external) */
	unsigned long long	statsPending;	/* Pending statistics to be flushed to the stats buffer */
# endif

# if HMALLOC_PROFILE
	size_t	sampleRate;			/* Sampling rate for profiling (e.g., 1 in N allocations) */
	size_t	sampleCountdown;	/* Countdown to the next sample allocation */
# endif
};

/* ----- Heap / Subproc / TLD ----- */

/**
 * @struct s_hmallocHeap
 * @brief A heap structure that manages multiple theaps and arenas for a process.
 *
 * Each heap can manage multiple theaps (per-thread heaps) and arenas.
 * The heap maintains a list of theaps, a lock for thread-safe access,
 * and counters for abandoned pages per bin. It also tracks the memory
 * ID for the heap's memory region and may contain statistics and profiling
 * information if enabled.
 */
struct s_hmallocHeap {
	t_hmallocSubproc	*subproc;	/* Pointer to the owning subproc (may be NULL if abandoned) */
	t_hmallocHeap		*next;		/* Next heap in the subproc's list of heaps */
	t_hmallocHeap		*prev;		/* Previous heap in the subproc's list of heaps */

	t_hmallocTheap		*theaps;	/* Pointer to the list of theaps (per-thread heaps) managed by this heap */
	t_hmallocLock		theapsLock;	/* Lock for thread-safe access to the theaps list */

	_Atomic size_t		abandonedCountBin[HMALLOC_BIN_COUNT];	/* Count of abandoned pages per bin (O(1) allocation) */

	t_hmallocMemId	memid;	/* Memory ID for the heap's memory region */

# if HMALLOC_STATS
	t_hmallocStats	statsStorage;	/* Stats of heaps created with hmHeapNew */
	t_hmallocStats	*stats;			/* statsStorage, or g_hmalloc.statsHeapMain */
# endif

# if HMALLOC_PROFILE
	t_hmallocProfiler	*profiler;
# endif
};

/**
 * @typedef t_hmMsecs
 * @brief A type representing milliseconds as a signed 64-bit integer.
 *
 * This type is used to represent time in milliseconds, allowing for
 * high-resolution timing and duration measurements in the memory allocator.
 */
typedef int64_t	t_hmMsecs;

/**
 * @struct s_hmallocSubproc
 * @brief A subproc structure that manages multiple heaps and arenas for a process.
 *
 * Each subproc can manage multiple heaps (t_hmallocHeap) and arenas (t_hmallocArena).
 * The subproc maintains a list of heaps, a lock for thread-safe access, and counters
 * for the number of threads and heaps. It also tracks the memory ID for the subproc's
 * memory region and may contain statistics if enabled.
 */
struct s_hmallocSubproc {
	_Atomic size_t				arenaCount;					/* Count of arenas managed by this subproc */
	t_hmallocLock				arenaReserveLock;			/* Serialises arena creation (readers are lock-free) */
	_Atomic(t_hmallocArena *)	arenas[HMALLOC_MAX_ARENAS];	/* Array of pointers to arenas managed by this subproc */

	_Atomic(t_hmallocHeap *)	heapMain;	/* Pointer to the main heap for this subproc */
	t_hmallocHeap				*heaps;		/* Pointer to the list of heaps managed by this subproc */
	t_hmallocLock				heapsLock;	/* Lock for thread-safe access to the heaps list */

	_Atomic size_t			threadCount;	/* Count of threads managed by this subproc */
	_Atomic size_t			threadTotalCount;	/* Threads ever created (source of tld->threadSeq) */
	_Atomic size_t			heapCount;		/* Count of heaps managed by this subproc */

	_Atomic t_hmMsecs		purgeExpire;	/* Earliest arena purge expiration (0: none pending) */

	t_hmallocMemId	memid;	/* Memory ID for the subproc's memory region */

# if HMALLOC_STATS
	t_hmallocStats	*stats;
# endif
};

/**
 * @struct s_hmallocTld
 * @brief A thread-local data structure that holds per-thread information.
 *
 * The TLD contains a pointer to the thread's subproc, the thread's theaps,
 * a random number generator, and a memory ID.
 */
struct s_hmallocTld {
	t_hmallocThreadId	threadId;	/* Thread ID for this thread */
	size_t				threadSeq;	/* Creation sequence number, spaces out bitmap searches */
	t_hmallocSubproc	*subproc;	/* Pointer to the subproc this thread belongs to */
	t_hmallocTheap		*theaps;	/* Pointer to the list of theaps (per-thread heaps) for this thread */

	uint64_t	rng[2];		/* Random number generator state for this thread */

	size_t		forkEpoch;	/* Fork epoch at creation: older means the thread is dead in a forked child */
	bool		inProfiler;	/* Inside a profiler callback: do not sample (no recursion) */

	t_hmallocMemId	memid;	/* Memory ID for the TLD's memory region */
};

/**
 * @enum e_hmallocCategory
 * @brief An enumeration of the different categories of memory allocations.
 */
typedef enum e_hmallocCategory {
	HMALLOC_CAT_TINY	= 0,
	HMALLOC_CAT_SMALL	= 1,
	HMALLOC_CAT_LARGE	= 2
} t_hmallocCategory;

/* ----- Arena bitmaps ----- */

/**
 * @brief The number of 64-bit words needed to represent the arena's bitmap.
 */
# define HMALLOC_ARENA_BITMAP_WORDS \
	((HMALLOC_ARENA_SLICE_COUNT + 63) / 64)

/**
 * @brief The number of 64-bit words needed to represent the arena's metadata.
 */
# define HMALLOC_PAGE_META_COUNT	HMALLOC_ARENA_SLICE_COUNT

/* ----- Time ----- */

/**
 * @brief Get the current time in milliseconds.
 *
 * This function returns the current time in milliseconds since the epoch.
 * It is used for timing and profiling purposes in the memory allocator.
 *
 * @return The current time in milliseconds as a t_hmMsecs value.
 */
t_hmMsecs	hmallocPrimClockNow(void);


/* ----- Arena ----- */

/**
 * @struct s_hmallocArena
 * @brief A memory arena that manages a contiguous region of memory.
 *
 * Each arena is a contiguous region of memory that is divided into slices.
 * The arena manages the allocation and deallocation of pages within its slices,
 * and maintains bitmaps to track free slices, abandoned pages, and purgeable slices.
 *
 * The arena also tracks the owning subproc, the arena index, and the number of
 * slices in use. It provides thread-safe access to its data structures using
 * atomic operations.
 */
struct s_hmallocArena {
	_Atomic(t_hmallocPage *)	metas[HMALLOC_PAGE_META_COUNT];	/* Metadata pages for each slice in the arena */

	t_hmallocMemId		memid;		/* Memory ID for the arena's memory region */
	t_hmallocSubproc	*subproc;	/* Pointer to the owning subproc (may be NULL if abandoned) */
	size_t				arenaIdx;	/* Index of this arena in the subproc's arenas array */
	void				*start;		/* Start address of the arena's memory region */
	size_t				sliceCount;	/* Total number of slices in the arena */
	size_t				infoSlices;	/* Number of slices used for metadata and bitmaps (reserved) */

	_Atomic size_t		freeSlices;	/* Count of free slices in the arena (O(1) allocation) */
	/* Count of abandoned pages per bin (O(1) allocation) */
	_Atomic size_t		abandonedCount[HMALLOC_BIN_COUNT];

	t_hmallocCategory	category;	/* Category of the arena (tiny, small, large) */

	t_hmBBitmap			slicesFree;	/* Bitmap of free slices in the arena */
	/* Slices that have ever been handed out (no longer known to be zero) */
	t_hmBitmap			slicesDirty;
	/* Per bin: slices holding an abandoned page that can be reclaimed */
	t_hmBitmap			pagesAbandoned[HMALLOC_BIN_COUNT];

	_Atomic t_hmMsecs	purgeExpire;	/* Expiration time for purging the arena's slices (in milliseconds) */
	/* Bitmap of slices that are eligible for purging (not used for a while) */
	_Atomic uint64_t	purgeBitmap[HMALLOC_ARENA_BITMAP_WORDS];
};

/* ----- Errno fallbacks ----- */

# ifndef EAGAIN
#  define EAGAIN	11
# endif
# ifndef ENOMEM
#  define ENOMEM	12
# endif
# ifndef EFAULT
#  define EFAULT	14
# endif
# ifndef EINVAL
#  define EINVAL	22
# endif
# ifndef EOVERFLOW
#  define EOVERFLOW	75
# endif

#endif /* HMALLOC_TYPES_H */
