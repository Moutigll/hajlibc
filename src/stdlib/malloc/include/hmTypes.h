/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file hmTypes.h
 * @brief Core types of the hmalloc allocator.
 * @Created: 2026/10/05 15:26:28 by Moutig
 * @Updated: 2026/10/16 08:58:07 by Moutig
 *
 * Pages, blocks, heaps, sub-processes, thread-local heaps and
 * arenas. The layout is designed for cache-line locality:
 * hot fields (free-list head, thread id, keys) are grouped
 * together, and the cross-thread free-list is on its own
 * cache line to avoid false sharing.
 *
 * The `reserved` / `capacity` distinction follows mimalloc:
 * `reserved` is the total number of blocks the zone can hold,
 * `capacity` is the number of blocks that have already been
 * linked into the page's free-list. Extension is lazy.
 */
/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file hmTypes.h
 * @brief Core types of the hmalloc allocator.
 */

#ifndef HMALLOC_TYPES_H
# define HMALLOC_TYPES_H

# include <stdint.h>
# include <stdbool.h>
# include <limits.h>

#include "../../../../include/malloc.h"

# include "hmBits.h"
# include "hmAtomic.h"

/* ------------------------------------------------------------------ */
/*  Géométrie arène                                                    */
/* ------------------------------------------------------------------ */

/*
 * Arène alignée 256 MiB, taille fixe. Chaque arène contient :
 *   - slice 0..infoSlices-1 : metas[] + struct t_hmallocArena + bitmaps
 *   - slice infoSlices..N-1 : slices utilisables
 *
 * Le lookup page<->adresse se fait via un tableau de pointeurs
 * alignés au début de l'arène (metas). Comme l'arène est 256 MiB
 * alignée et fait exactement 256 MiB, un simple masque sur
 * l'adresse retrouve le début de l'arène sans dépendre d'aucun
 * autre pointeur (contrairement au `sliceMap` de l'ancien code).
 */
#ifndef HMALLOC_ARENA_SIZE
# define HMALLOC_ARENA_SIZE   (256 * HMALLOC_MIB)   /* 256 MiB */
#endif
#define HMALLOC_ARENA_MASK    (HMALLOC_ARENA_SIZE - 1)

#ifndef HMALLOC_SLICE_SHIFT
# define HMALLOC_SLICE_SHIFT    16                  /* 64 KiB */
#endif
#define HMALLOC_SLICE_SIZE            ((size_t)1 << HMALLOC_SLICE_SHIFT)
#define HMALLOC_ARENA_SLICE_COUNT     (HMALLOC_ARENA_SIZE >> HMALLOC_SLICE_SHIFT)
                                                                 /* 4096 */
/* FIX perf (2) : nombre d'arènes vides gardées par catégorie avant de
 * détruire. mimalloc ne détruit jamais à chaud ; on garde un petit
 * cache pour éviter le cycle munmap/mmap à chaque pic alloc/free. */
#ifndef HMALLOC_MAX_EMPTY_ARENAS
# define HMALLOC_MAX_EMPTY_ARENAS 4u
#endif
/* ------------------------------------------------------------------ */
/*  Classes de pages (identiques à mimalloc)                           */
/* ------------------------------------------------------------------ */

#define HMALLOC_SMALL_PAGE_SIZE     (64 * 1024)                    /* 64 KiB  */
#define HMALLOC_MEDIUM_PAGE_SIZE    (8 * HMALLOC_SMALL_PAGE_SIZE)  /* 512 KiB */
#define HMALLOC_LARGE_PAGE_SIZE     (8 * HMALLOC_MEDIUM_PAGE_SIZE)  /* 4 MiB  */
#define HMALLOC_LARGE_MAX_OBJ_SIZE  (HMALLOC_LARGE_PAGE_SIZE / 2)   /* 2 MiB  */
#define HMALLOC_LARGE_MAX_OBJ_WSIZE (HMALLOC_LARGE_MAX_OBJ_SIZE / 8) /* 256 Ki */

#define HMALLOC_SMALL_MAX_OBJ_SIZE  ((HMALLOC_SMALL_PAGE_SIZE  - 4096) / 6)   /* 10240  */
#define HMALLOC_MEDIUM_MAX_OBJ_SIZE ((HMALLOC_MEDIUM_PAGE_SIZE - 4096) / 6)   /* 86656  */

/* Le "SMALL" historique devient la borne des bins (== LARGE_MAX) */
#define HMALLOC_SMALL_MAX_SIZE      HMALLOC_LARGE_MAX_OBJ_SIZE


#define HMALLOC_MAX_ARENAS 64

/* ----- Free-list encoding ----- */
typedef uintptr_t	t_hmEncoded;
typedef size_t		t_hmallocThreadId;

typedef struct s_hmallocBlock {
	t_hmEncoded	next;
} t_hmallocBlock;

/* ------------------------------------------------------------------ */
/*  Page                                                               */
/* ------------------------------------------------------------------ */

struct s_hmallocTheap;
struct s_hmallocHeap;
struct s_hmallocArena;

typedef struct s_hmallocPage {
	/* ---- Cache line 0 : champs chauds, lus à chaque alloc/free ---- */
	_Atomic t_hmallocThreadId	xthreadId;	/* 0  : tid | flags */
	t_hmallocBlock				*free;		/* 8  : free list owner */
	t_hmallocBlock				*localFree;	/* 16 : frees owner non mergés */

	size_t		blockSize;		/* 24 */
	size_t		pageOffset;		/* 32 : offset vers 1er bloc */
	uint16_t	capacity;		/* 40 */
	uint16_t	reserved;		/* 42 */
	uint16_t	used;			/* 44 */
	uint8_t		retireExpire;	/* 46 */
	bool		freeIsZero;		/* 47 */
	uint64_t	_pad_hot[2];	/* 48..63 : aligne xthreadFree */

	/* ---- Cache line 1 : xthreadFree seul (évite le false sharing) ---- */
	_Atomic uintptr_t			xthreadFree;	/* 64 */
	uint64_t					_pad_xthread_free[7];	/* 72..127 : padding */

	/* ---- Cache line 2 : champs froids (métadonnées) ---- */
	uintptr_t					keys[2];		/* 128..143 */
	struct s_hmallocTheap		*theap;			/* 144 */
	struct s_hmallocHeap		*heap;			/* 152 */
	struct s_hmallocArena		*arena;			/* 160 */
	struct s_hmallocPage		*next;			/* 168 */
	struct s_hmallocPage		*prev;			/* 176 */
char						_pad_cold[8];	/* 184..191 : padding */
} t_hmallocPage;

_Static_assert(offsetof(t_hmallocPage, xthreadFree) % 64 == 0,
               "xthreadFree doit être sur sa propre cache line");
_Static_assert(sizeof(t_hmallocPage) % 64 == 0,
               "t_hmallocPage doit être multiple de 64 pour l'alignement");

/* ----- Page flags ----- */
# define HMALLOC_PAGE_IN_FULL_QUEUE			((size_t)0x1)
# define HMALLOC_PAGE_HAS_INTERIOR_POINTERS	((size_t)0x2)
# define HMALLOC_PAGE_FLAG_MASK				((size_t)0x3)

# define HMALLOC_THREADID_ABANDONED			((size_t)0)

typedef struct s_hmallocPageQueue {
	t_hmallocPage	*first;
	t_hmallocPage	*last;
	size_t			count;
	size_t			blockSize;
} t_hmallocPageQueue;

/* ----- MemId ----- */
typedef enum e_hmallocMemKind {
	HMALLOC_MEM_NONE,
	HMALLOC_MEM_EXTERNAL,
	HMALLOC_MEM_STATIC,
	HMALLOC_MEM_OS,
	HMALLOC_MEM_OS_HUGE,
	HMALLOC_MEM_ARENA,
	HMALLOC_MEM_MALLOC
} t_hmallocMemKind;

typedef struct s_hmallocArena	t_hmallocArena;

typedef struct s_hmallocMemId {
	union {
		struct { void *base; size_t size; } os;
		struct { t_hmallocArena *arena; uint32_t sliceIndex; uint32_t sliceCount; } arena;
		struct { void *base; size_t size; } malloc;
	} mem;
	t_hmallocMemKind	memkind;
	bool				isPinned;
	bool				initiallyCommitted;
	bool				initiallyZero;
} t_hmallocMemId;

typedef struct s_hmallocHeap	t_hmallocHeap;
typedef struct s_hmallocTheap	t_hmallocTheap;
typedef struct s_hmallocTld		t_hmallocTld;
typedef struct s_hmallocSubproc	t_hmallocSubproc;
typedef struct s_hmallocProfiler	t_hmallocProfiler;

/* ----- Bins ----- */


/* ------------------------------------------------------------------ */
/*  Theap                                                              */
/* ------------------------------------------------------------------ */

struct s_hmallocTheap {
	t_hmallocPage	*pagesFreeDirect[HMALLOC_PAGES_DIRECT];
	t_hmallocArena	*arenaHint[3];

	t_hmallocTld					*tld;
	_Atomic(t_hmallocHeap *)		heap;
	_Atomic(t_hmallocSubproc *)		subproc;
	_Atomic size_t					refcount;

	long	pageFullRetain;
	bool	allowPageReclaim;
	bool	allowPageAbandon;
	bool	isDetached;

	size_t	sampleCountdown;
	size_t	sampleRate;
	size_t	profileSampleRate;
	size_t	guardedSampleRate;

	unsigned long long	heartbeat;
	size_t				pageCount;
	size_t				pagesFullSize;

	/* FIX 6 : compteur de full pages retenues par bin */
	uint16_t			pagesFullCount[HMALLOC_BIN_COUNT];
		/* FIX perf (4) : compteur de pages retirées par bin, pour rendre
	 * hmallocPageRetire O(1) au lieu de O(pages). mimalloc fait pareil
	 * avec page_retired_min/max. */
	uint16_t			pagesRetiredCount[HMALLOC_BIN_COUNT];
	uint16_t			retiredMinBin;
	uint16_t			retiredMaxBin;
	uint16_t			_pad_retired;

	t_hmallocTheap	*tnext;
	t_hmallocTheap	*tprev;
	t_hmallocTheap	*hnext;
	t_hmallocTheap	*hprev;

	t_hmallocPageQueue	pages[HMALLOC_BIN_COUNT];
	t_hmallocMemId		memid;

	t_hmallocStats		statsStorage;
	t_hmallocStats		*stats;
	unsigned long long	statsPending;
};

/* ------------------------------------------------------------------ */
/*  Heap / Subproc / TLD                                               */
/* ------------------------------------------------------------------ */

struct s_hmallocHeap {
	t_hmallocSubproc	*subproc;
	size_t				heapSeq;
	t_hmallocHeap		*next;
	t_hmallocHeap		*prev;

	t_hmallocTheapKey	theapKey;
	t_hmallocArena		*exclusiveArena;
	int					numaNode;

	t_hmallocTheap		*theaps;
	t_hmallocLock		theapsLock;

		/* FIX PERF : compteur d'abandonnés par bin, pour early-exit
	 * dans hmallocTheapFindPage (évite de scanner toutes les arènes
	 * quand aucun abandonné n'existe pour ce bin). */
	_Atomic size_t		abandonedCountBin[HMALLOC_BIN_COUNT];
	t_hmallocPage		*osAbandonedPages;
	t_hmallocLock		osAbandonedPagesLock;

	t_hmallocMemId			memid;
	struct s_hmallocStats	*stats;
	struct s_hmallocProfiler	*profiler;
};

struct s_hmallocSubproc {
	size_t				subprocSeq;
	t_hmallocSubproc	*next;
	t_hmallocSubproc	*prev;

	_Atomic size_t			arenaCount;
	_Atomic(t_hmallocArena *)arenas[HMALLOC_MAX_ARENAS];
	t_hmallocLock			arenaReserveLock;

	_Atomic(t_hmallocHeap *)	heapMain;
	t_hmallocHeap				*heaps;
	t_hmallocLock				heapsLock;

	_Atomic size_t			threadCount;
	_Atomic size_t			heapCount;

	t_hmallocMemId			memid;
	struct s_hmallocStats	*stats;
};

struct s_hmallocTld {
	t_hmallocThreadId	threadId;
	size_t				threadSeq;
	int					numaNode;
	t_hmallocSubproc	*subproc;
	t_hmallocTheap		*theaps;
	t_hmallocLock		theapsLock;
	bool				recurse;
	bool				isInThreadpool;

	uint64_t	rng[2];

	t_hmallocMemId		memid;
};

typedef enum e_hmallocCategory {
	HMALLOC_CAT_TINY	= 0,
	HMALLOC_CAT_SMALL	= 1,
	HMALLOC_CAT_LARGE	= 2
} t_hmallocCategory;

/* ------------------------------------------------------------------ */
/*  Arène                                                              */
/* ------------------------------------------------------------------ */

/* Nombre de bits par bitmap de slices (4096/64). */
#define HMALLOC_ARENA_BITMAP_WORDS \
	((HMALLOC_ARENA_SLICE_COUNT + 63) / 64)

/* Les metas = un pointeur par slice, alignés au début de l'arène. */
#define HMALLOC_PAGE_META_COUNT     HMALLOC_ARENA_SLICE_COUNT
#define HMALLOC_PAGE_META_BYTES     (HMALLOC_PAGE_META_COUNT * sizeof(t_hmallocPage *))

/* ------------------------------------------------------------------ */
/*  Arène                                                              */
/* ------------------------------------------------------------------ */
/**
 * @brief Monotonic timestamp in milliseconds.
 */
typedef int64_t	t_hmMsecs;

/**
 * @brief Return a monotonic timestamp.
 * @return Milliseconds since an arbitrary point.
 */
t_hmMsecs	hmallocPrimClockNow(void);

/* ------------------------------------------------------------------ */
/*  Bitmap d'arène à deux niveaux (chunkmap + chunks binnés)           */
/* ------------------------------------------------------------------ */
/*
 * L'arène est découpée en "chunks" de HMALLOC_BCHUNK_BITS slices.
 * Chaque chunk a 8 champs de 64 bits (= 512 bits).
 *
 * Un premier bitmap (chunkmap) indique pour chaque chunk s'il a
 * au moins un bit libre (heuristique conservatrice : le bit est
 * mis à 1 quand on alloue, il n'est effacé que si le chunk est
 * réellement vide).
 *
 * Cinq chunkmaps supplémentaires (chunkmap_bins) étiquettent chaque
 * chunk avec la classe de taille de la première allocation qui y
 * a été faite. Cela force les pages de même type (SMALL, MEDIUM,
 * LARGE) à se regrouper physiquement dans l'arène, ce qui évite
 * la fragmentation et permet de libérer des plages contiguës.
 *
 * Classes : SMALL (1 slice), MEDIUM (8), LARGE (64), HUGE (>512),
 * OTHER (tout le reste), NONE (non étiqueté).
 */

#ifndef HMALLOC_BCHUNK_BITS
# define HMALLOC_BCHUNK_BITS     512u
#endif

#define HMALLOC_BCHUNK_FIELDS \
    (HMALLOC_BCHUNK_BITS / 64u)                            /* 8  */
#define HMALLOC_BCHUNK_COUNT  \
    (HMALLOC_ARENA_SLICE_COUNT / HMALLOC_BCHUNK_BITS)      /* 8  */
#define HMALLOC_BCHUNKMAP_WORDS \
    ((HMALLOC_BCHUNK_COUNT + 63u) / 64u)                   /* 1  */

typedef enum e_hmChunkBin {
    HMALLOC_CBIN_SMALL  = 0,   /* slice_count == 1        */
    HMALLOC_CBIN_MEDIUM = 1,   /* slice_count == 8        */
    HMALLOC_CBIN_LARGE  = 2,   /* slice_count == 64       */
    HMALLOC_CBIN_HUGE   = 3,   /* slice_count > 512       */
    HMALLOC_CBIN_OTHER  = 4,   /* tout autre slice_count  */
    HMALLOC_CBIN_NONE   = 5,   /* chunk non étiqueté      */
    HMALLOC_CBIN_COUNT  = 6
} t_hmChunkBin;

/* Un chunk = HMALLOC_BCHUNK_FIELDS champs atomiques de 64 bits. */
typedef struct s_hmBChunk {
    _Atomic uint64_t    fields[HMALLOC_BCHUNK_FIELDS];
} t_hmBChunk;

/* Bitmap binné : chunkmap + chunkmap_bins + chunks. */
typedef struct s_hmBBitmap {
    _Atomic uint64_t    chunkmap[HMALLOC_BCHUNKMAP_WORDS];
    _Atomic uint64_t    chunkmap_bins[HMALLOC_CBIN_COUNT - 1]
                                      [HMALLOC_BCHUNKMAP_WORDS];
    t_hmBChunk          chunks[HMALLOC_BCHUNK_COUNT];
} t_hmBBitmap;

/* Compteurs atomiques par bin, pour early-exit. */
typedef struct s_hmBBitmapCounters {
    _Atomic size_t      abandonedCount[HMALLOC_BIN_COUNT];
} t_hmBBitmapCounters;

#define HMALLOC_ARENA_BITMAP_WORDS \
	((HMALLOC_ARENA_SLICE_COUNT + 63) / 64)

#define HMALLOC_PAGE_META_COUNT     HMALLOC_ARENA_SLICE_COUNT
#define HMALLOC_PAGE_META_BYTES     (HMALLOC_PAGE_META_COUNT * sizeof(t_hmallocPage *))


/* ------------------------------------------------------------------ */
/*  Bitmap d'arène à deux niveaux (chunkmap + chunks binnés)           */
/* ------------------------------------------------------------------ */


#define HMALLOC_BCHUNK_WORDS          (HMALLOC_BCHUNK_BITS / 64u)        /* 8 */
#define HMALLOC_BCHUNK_COUNT \
    (HMALLOC_ARENA_SLICE_COUNT / HMALLOC_BCHUNK_BITS)                    /* 8 */
#define HMALLOC_CHUNKMAP_WORDS \
    ((HMALLOC_BCHUNK_COUNT + 63u) / 64u)                                 /* 1 */



struct s_hmallocArena {
	/* metas[0..sliceCount-1] : tableau de pointeurs vers les pages. */
	_Atomic(t_hmallocPage *) metas[HMALLOC_PAGE_META_COUNT];

	/* Header */
	t_hmallocMemId		memid;
	t_hmallocSubproc	*subproc;
	size_t				arenaIdx;
	void				*start;
	size_t				sliceCount;
	size_t				infoSlices;

	/* FIX: champs modifiables depuis plusieurs threads -> atomiques. */
	_Atomic size_t		freeSlices;
	_Atomic size_t		firstFreeWord;
	_Atomic size_t		failedN;
	_Atomic size_t		abandonedCount[HMALLOC_BIN_COUNT];  /* NEW: compteur par bin */

	t_hmallocLock		sliceLock;
	int					numaNode;
	bool				isExclusive;
	size_t				totalSize;
	t_hmallocArena		*parent;
	t_hmallocArena		*next;
	t_hmallocCategory	category;

	t_hmBBitmap         slicesFree;         /* NOUVEAU : bitmap binné */
	_Atomic uint64_t	pages_abandoned[HMALLOC_BIN_COUNT]
	                                    [HMALLOC_ARENA_BITMAP_WORDS];
	/* hmTypes.h - struct s_hmallocArena */
_Atomic t_hmMsecs   purgeExpire;                      /* NEW */
_Atomic uint64_t    purgeBitmap[HMALLOC_ARENA_BITMAP_WORDS];  /* NEW */
_Atomic size_t      emptyArenaLarge;                  /* NEW (optionnel, cf. plus bas) */
};

/* ----- Error codes ----- */
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
