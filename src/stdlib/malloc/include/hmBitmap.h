/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file hmBitmap.h
 * @brief Two-level and binned bitmaps for tracking abandoned pages and free slices.
 * @Created: 2026/11/16 16:04:13 by Moutig
 * @Updated: 2026/11/16 16:04:30 by Moutig
 *
 * Layout:
 *
 *   t_hmBField   one machine word of bits (size_t).
 *   t_hmBChunk   HMALLOC_BCHUNK_FIELDS bfields (512 bits on 64-bit, 256 on
 *                32-bit), cache aligned. A range of bits never spans more
 *                than what the search functions below support.
 *   chunkmap     a chunk with one bit per chunk, set when that chunk may
 *                have a bit set. It is conservative: a set bit with an
 *                all-clear chunk is fine, and a clear bit is only allowed
 *                briefly while a setter is about to set it. Clearing it
 *                checks the chunk, clears the bit, then checks again.
 *
 *   t_hmBitmap   chunkmap + chunks (abandoned pages).
 *   t_hmBBitmap  chunkmap + per size-class chunkmaps + chunks (free
 *                slices). Each chunk is assigned to one size class once
 *                its first slice is allocated, so that small pages do not
 *                fragment chunks used by larger ones.
 *
 * The bitmaps embedded in an arena have a fixed number of chunks
 * (HMALLOC_ARENA_CHUNK_COUNT); `chunkCount` still bounds every access.
 */

#ifndef HMALLOC_BITMAP_H
# define HMALLOC_BITMAP_H

# include <stdbool.h>
# include <stddef.h>
# include <stdint.h>
# include <stdatomic.h>

# include "hmBits.h"
# include "../../../../include/malloc.h"	/* t_hmallocChunkBin */

/* ----- Arena geometry ----- */

/*
 * 256 MiB aligned arena, fixed size. It is then cut into 64 KiB slices,
 * each slice containing a single page.
 * - Slice 0 is reserved for the arena's metadata.
 * - Slice 1 contains the struct t_hmallocArena + metas[] + bitmaps
 * - Slice 2..N < 4095 are used for pages.
 *
 * Three categories of pages are supported:
 * - Tiny,	64 KiB	-> 1 slice,	for blocks up to 1 KiB
 * - Small,	512 KiB	-> 8 slices,	for blocks up to 4 KiB
 * - Large,	4 MiB	-> 64 slices,	for blocks up to 2 MiB
 *
 * Each category has its own dedicated arena list to avoid fragmentation.
 *
 * The arena uses three bitmaps(table where 1bit = a slice) to track free slices:
 * - slicesFree: a 2-level bitmap of free slices, used to find a free slice in O(1).
 * 		- chunkmap: 1 bit per 512 slices, set if any slice in that chunk is free.
 * 		- chunkmapBins: tags each chunk with a bin (small/medium/large/huge/other/none) for O(1) category selection.
 * 		- chunks[8]: each chunk has 512 bits, one per slice, set if the slice is free.
 * - pagesAbandoned: a bitmap per bin, set if the page is abandoned (freed by another thread).
 * - purgeBitmap: a bitmap of slices that are eligible for purging (not used for a while).
 */
# ifndef HMALLOC_ARENA_SIZE
#  define HMALLOC_ARENA_SIZE	(256 * HMALLOC_MIB)	/* 256 MiB */
# endif

/**
 * @brief Mask for the arena size.
 * This is used to extract the arena offset from an address.
 */
# define HMALLOC_ARENA_MASK		(HMALLOC_ARENA_SIZE - 1)

/**
 * @brief Shift for the slice size.
 * This is used to calculate the slice size.
 */
# ifndef HMALLOC_SLICE_SHIFT
#  define HMALLOC_SLICE_SHIFT	16					/* 64 KiB */
# endif

# define HMALLOC_SLICE_SIZE			((size_t)1 << HMALLOC_SLICE_SHIFT)
# define HMALLOC_ARENA_SLICE_COUNT	(HMALLOC_ARENA_SIZE >> HMALLOC_SLICE_SHIFT)
# define HMALLOC_MAX_ARENAS			64

/* ----- Geometry ----- */

typedef size_t	t_hmBField;

# define HMALLOC_BFIELD_BITS_SHIFT	(HMALLOC_SIZE_SHIFT + 3)
# define HMALLOC_BFIELD_BITS		((size_t)1 << HMALLOC_BFIELD_BITS_SHIFT)
# define HMALLOC_BFIELD_SIZE		(HMALLOC_BFIELD_BITS / 8)
# define HMALLOC_BFIELD_LO_BIT8		((~(t_hmBField)0) / 0xFF)		/* 0x0101.. */
# define HMALLOC_BFIELD_HI_BIT8		(HMALLOC_BFIELD_LO_BIT8 << 7)	/* 0x8080.. */

# define HMALLOC_BCHUNK_FIELDS		8u
# define HMALLOC_BCHUNK_BITS		(HMALLOC_BCHUNK_FIELDS * HMALLOC_BFIELD_BITS)
# define HMALLOC_BCHUNK_SIZE		(HMALLOC_BCHUNK_BITS / 8)

# define HMALLOC_BCHUNKMAP_BITS			HMALLOC_BCHUNK_BITS
# define HMALLOC_BITMAP_MAX_CHUNK_COUNT	HMALLOC_BCHUNKMAP_BITS
# define HMALLOC_BITMAP_MIN_CHUNK_COUNT	1u

/* Chunks needed for one bit per arena slice. */
# define HMALLOC_ARENA_CHUNK_COUNT \
	((HMALLOC_ARENA_SLICE_COUNT + HMALLOC_BCHUNK_BITS - 1) / HMALLOC_BCHUNK_BITS)

_Static_assert(HMALLOC_ARENA_CHUNK_COUNT >= HMALLOC_BITMAP_MIN_CHUNK_COUNT
	&& HMALLOC_ARENA_CHUNK_COUNT <= HMALLOC_BITMAP_MAX_CHUNK_COUNT,
	"arena chunk count out of bitmap bounds");

/**
 * @struct s_hmBChunk
 * @brief A chunk of bits in the bitmap.
 */
typedef struct s_hmBChunk {
	_Alignas(HMALLOC_BCHUNK_SIZE)
	_Atomic(t_hmBField)	bfields[HMALLOC_BCHUNK_FIELDS];
} t_hmBChunk;

typedef t_hmBChunk	t_hmBChunkmap;

/* Size classes of a chunk in the binned bitmap (search order): see the
 * public t_hmallocChunkBin in <malloc.h>. */
typedef t_hmallocChunkBin	t_hmChunkBin;

/**
 * @brief Get the chunk bin for a given slice count.
 *
 * This function returns the appropriate chunk bin for a given slice count.
 * It is used to categorize chunks in the binned bitmap based on their size.
 *
 * @param sliceCount The number of slices in the chunk.
 * @return The corresponding chunk bin.
 */
static inline t_hmChunkBin	hmChunkBinOf(size_t sliceCount)
{
	if (sliceCount == 1)
		return (HMALLOC_CBIN_SMALL);
	if (sliceCount == 8)
		return (HMALLOC_CBIN_MEDIUM);
	if (sliceCount == HMALLOC_BFIELD_BITS)
		return (HMALLOC_CBIN_LARGE);
	if (sliceCount > HMALLOC_BCHUNK_BITS)
		return (HMALLOC_CBIN_HUGE);
	return (HMALLOC_CBIN_OTHER);
}

/**
 * @struct s_hmBitmap
 * @brief A two-level bitmap for tracking abandoned pages.
 *
 * This struct represents a two-level bitmap used to track abandoned pages
 * in the memory allocator. It consists of a chunk map and an array of chunks,
 * where each chunk contains a fixed number of bits representing the state of
 * slices in the arena.
 */
typedef struct s_hmBitmap {
	_Atomic size_t	chunkCount;		/* 0 < count <= HMALLOC_BITMAP_MAX_CHUNK_COUNT */
	t_hmBChunkmap	chunkmap;
	t_hmBChunk		chunks[HMALLOC_ARENA_CHUNK_COUNT];
} t_hmBitmap;

/**
 * @struct s_hmBBitmap
 * @brief A binned bitmap for tracking free slices.
 *
 * This struct represents a binned bitmap used to track free slices in the
 * memory allocator. It consists of a chunk map, an array of per-size-class
 * chunk maps, and an array of chunks, where each chunk contains a fixed
 * number of bits representing the state of slices in the arena.
 */
typedef struct s_hmBBitmap {
	_Atomic size_t	chunkCount;
	_Atomic size_t	chunkMaxAccessed;	/* highest chunk index ever set or cleared */
	t_hmBChunkmap	chunkmap;
	t_hmBChunkmap	chunkmapBins[HMALLOC_CBIN_COUNT - 1];	/* excluding NONE */
	t_hmBChunk		chunks[HMALLOC_ARENA_CHUNK_COUNT];
} t_hmBBitmap;

/* ----- Bitmap (abandoned pages) ----- */

/**
 * @brief Initialize a bitmap.
 *
 * This function initializes a bitmap by setting its chunk count to zero
 * and clearing all bits in the chunk map and chunks.
 *
 * @param bitmap Pointer to the bitmap to initialize.
 */
void	hmBitmapInit(t_hmBitmap *bitmap, size_t bitCount);

/**
 * @brief Set multiple bits in a bitmap.
 *
 * This function sets multiple bits in a bitmap and returns the number of bits that were already set.
 *
 * @param bitmap Pointer to the bitmap.
 * @param idx The starting index of the bits to set.
 * @param n The number of bits to set.
 * @param alreadySet Pointer to a variable that will store the number of bits that were already set.
 * @return True if any bits were set, false otherwise.
 */
bool	hmBitmapSetN(t_hmBitmap *bitmap, size_t idx, size_t n, size_t *alreadySet);

/**
 * @brief Clear multiple bits in a bitmap.
 *
 * This function clears multiple bits in a bitmap and returns the number of bits that were already clear.
 *
 * @param bitmap Pointer to the bitmap.
 * @param idx The starting index of the bits to clear.
 * @param n The number of bits to clear.
 * @return True if any bits were cleared, false otherwise.
 */
bool	hmBitmapClearN(t_hmBitmap *bitmap, size_t idx, size_t n);

/**
 * @brief Check if multiple bits in a bitmap are set or clear.
 *
 * This function checks if multiple bits in a bitmap are set or clear and returns the result.
 *
 * @param set True if checking for set bits, false if checking for clear bits.
 * @param bitmap Pointer to the bitmap.
 * @param idx The starting index of the bits to check.
 * @param n The number of bits to check.
 * @return True if all specified bits match the expected state, false otherwise.
 */
bool	hmBitmapIsXsetN(bool set, t_hmBitmap *bitmap, size_t idx, size_t n);

/**
 * @brief Count the number of set bits in a bitmap.
 *
 * This function counts the number of set bits in a bitmap and returns the count.
 *
 * @param bitmap Pointer to the bitmap.
 * @return The number of set bits in the bitmap.
 */
size_t	hmBitmapPopcount(t_hmBitmap *bitmap);

static inline bool	hmBitmapSet(t_hmBitmap *bitmap, size_t idx)
{
	return (hmBitmapSetN(bitmap, idx, 1, NULL));
}

static inline bool	hmBitmapClear(t_hmBitmap *bitmap, size_t idx)
{
	return (hmBitmapClearN(bitmap, idx, 1));
}

/**
 * @brief Clear a bit in a bitmap only if it was previously set.
 *
 * This function clears a bit in a bitmap only if it was previously set.
 * It returns true if the bit was cleared, false if it was already clear.
 *
 * @param bitmap Pointer to the bitmap.
 * @param idx The index of the bit to clear.
 * @return True if the bit was cleared, false if it was already clear.
 */
void	hmBitmapClearOnceSet(t_hmBitmap *bitmap, size_t idx);

/**
 * @brief Claim callback for hmBitmapTryFindAndClaim.
 *
 * Called with the bit already cleared. Return true to keep it claimed.
 * On failure set `*keepSet` to restore the bit (the default).
 */
typedef bool	(t_hmClaimFun)(size_t sliceIdx, void *arg, bool *keepSet);

/**
 * @brief Find a set bit in a bitmap and claim it.
 *
 * This function finds a set bit in a bitmap starting from a given index,
 * clears it, and calls the provided claim callback. If the callback returns
 * false, the bit is restored to its original state.
 *
 * @param bitmap Pointer to the bitmap.
 * @param tseq The starting index for the search.
 * @param pidx Pointer to store the index of the claimed bit.
 * @param claim Callback function to handle the claimed bit.
 * @param arg Argument to pass to the claim callback.
 * @return True if a bit was claimed, false otherwise.
 */
bool	hmBitmapTryFindAndClaim(t_hmBitmap *bitmap, size_t tseq, size_t *pidx,
							t_hmClaimFun *claim, void *arg);

/* ----- Binned bitmap (free slices) ----- */

/**
 * @brief Initialize a binned bitmap.
 *
 * This function initializes a binned bitmap by setting its chunk count to zero
 * and clearing all bits in the chunk map, per-size-class chunk maps, and chunks.
 *
 * @param bbitmap Pointer to the binned bitmap to initialize.
 * @param bitCount The total number of bits in the binned bitmap.
 */
void	hmBBitmapInit(t_hmBBitmap *bbitmap, size_t bitCount);

/**
 * @brief Set multiple bits in a binned bitmap.
 *
 * This function sets multiple bits in a binned bitmap and returns true if any bits were set.
 *
 * @param bbitmap Pointer to the binned bitmap.
 * @param idx The starting index of the bits to set.
 * @param n The number of bits to set.
 * @return True if any bits were set, false otherwise.
 */
void	hmBBitmapUnsafeSetN(t_hmBBitmap *bbitmap, size_t idx, size_t n);

/**
 * @brief Set multiple bits in a binned bitmap atomically.
 *
 * This function sets multiple bits in a binned bitmap atomically and returns true if any bits were set.
 *
 * @param bbitmap Pointer to the binned bitmap.
 * @param idx The starting index of the bits to set.
 * @param n The number of bits to set.
 * @return True if any bits were set, false otherwise.
 */
bool	hmBBitmapSetN(t_hmBBitmap *bbitmap, size_t idx, size_t n);

/**
 * @brief Clear multiple bits in a binned bitmap atomically.
 *
 * This function clears multiple bits in a binned bitmap atomically and returns true if any bits were cleared.
 *
 * @param bbitmap Pointer to the binned bitmap.
 * @param idx The starting index of the bits to clear.
 * @param n The number of bits to clear.
 * @return True if any bits were cleared, false otherwise.
 */
bool	hmBBitmapTryClearNC(t_hmBBitmap *bbitmap, size_t idx, size_t n);

/**
 * @brief Check if multiple bits in a binned bitmap are set or clear.
 *
 * This function checks if multiple bits in a binned bitmap are set or clear and returns the result.
 *
 * @param set True if checking for set bits, false if checking for clear bits.
 * @param bbitmap Pointer to the binned bitmap.
 * @param idx The starting index of the bits to check.
 * @param n The number of bits to check.
 * @return True if all specified bits match the expected state, false otherwise.
 */
bool	hmBBitmapIsXsetN(bool set, t_hmBBitmap *bbitmap, size_t idx, size_t n);


/**
 * @brief Find and clear a set bit in a binned bitmap atomically.
 *
 * This function finds a set bit in a binned bitmap starting from a given index,
 * clears it atomically, and returns true if a bit was found and cleared.
 *
 * @param bbitmap Pointer to the binned bitmap.
 * @param tseq The starting index for the search.
 * @param pidx Pointer to store the index of the cleared bit.
 * @return True if a bit was found and cleared, false otherwise.
 */
bool	hmBBitmapTryFindAndClear(t_hmBBitmap *bbitmap, size_t tseq, size_t *pidx);

/**
 * @brief Find and clear a set bit in a binned bitmap atomically, optimized for 8 bits.
 *
 * This function finds a set bit in a binned bitmap starting from a given index,
 * clears it atomically, and returns true if a bit was found and cleared.
 * It is optimized for cases where the number of bits to check is 8.
 *
 * @param bbitmap Pointer to the binned bitmap.
 * @param tseq The starting index for the search.
 * @param pidx Pointer to store the index of the cleared bit.
 * @return True if a bit was found and cleared, false otherwise.
 */
bool	hmBBitmapTryFindAndClear8(t_hmBBitmap *bbitmap, size_t tseq, size_t *pidx);

/**
 * @brief Find and clear multiple set bits in a binned bitmap atomically.
 *
 * This function finds multiple set bits in a binned bitmap starting from a given index,
 * clears them atomically, and returns true if any bits were found and cleared.
 *
 * @param bbitmap Pointer to the binned bitmap.
 * @param tseq The starting index for the search.
 * @param n The number of bits to find and clear.
 * @param pidx Pointer to store the index of the first cleared bit.
 * @return True if any bits were found and cleared, false otherwise.
 */
bool	hmBBitmapTryFindAndClearNX(t_hmBBitmap *bbitmap, size_t tseq, size_t n, size_t *pidx);

/**
 * @brief Find and clear multiple set bits in a binned bitmap atomically, optimized for larger counts.
 *
 * This function finds multiple set bits in a binned bitmap starting from a given index,
 * clears them atomically, and returns true if any bits were found and cleared.
 * It is optimized for cases where the number of bits to check is larger than HMALLOC_BFIELD_BITS.
 *
 * @param bbitmap Pointer to the binned bitmap.
 * @param tseq The starting index for the search.
 * @param n The number of bits to find and clear.
 * @param pidx Pointer to store the index of the first cleared bit.
 * @return True if any bits were found and cleared, false otherwise.
 */
bool	hmBBitmapTryFindAndClearNC(t_hmBBitmap *bbitmap, size_t tseq, size_t n, size_t *pidx);

/**
 * @brief Find and clear multiple set bits in a binned bitmap atomically, for any count.
 *
 * This function finds multiple set bits in a binned bitmap starting from a given index,
 * clears them atomically, and returns true if any bits were found and cleared.
 * It is used for cases where the number of bits to check exceeds HMALLOC_BCHUNK_BITS.
 *
 * @param bbitmap Pointer to the binned bitmap.
 * @param tseq The starting index for the search.
 * @param n The number of bits to find and clear.
 * @param pidx Pointer to store the index of the first cleared bit.
 * @return True if any bits were found and cleared, false otherwise.
 */
bool	hmBBitmapTryFindAndClearN_(t_hmBBitmap *bbitmap, size_t tseq, size_t n, size_t *pidx);

/**
 * @brief Find and clear multiple set bits in a binned bitmap atomically, dispatching to the appropriate function.
 *
 * This function finds multiple set bits in a binned bitmap starting from a given index,
 * clears them atomically, and returns true if any bits were found and cleared.
 * It dispatches to the appropriate specialized function based on the number of bits to check.
 *
 * @param bbitmap Pointer to the binned bitmap.
 * @param tseq The starting index for the search.
 * @param n The number of bits to find and clear.
 * @param pidx Pointer to store the index of the first cleared bit.
 * @return True if any bits were found and cleared, false otherwise.
 */
static inline bool	hmBBitmapTryFindAndClearN(t_hmBBitmap *bbitmap, size_t tseq,
											  size_t n, size_t *pidx)
{
	if (n == 1)
		return (hmBBitmapTryFindAndClear(bbitmap, tseq, pidx));
	if (n == 8)
		return (hmBBitmapTryFindAndClear8(bbitmap, tseq, pidx));
	if (n == 0)
		return (false);
	if (n <= HMALLOC_BFIELD_BITS)
		return (hmBBitmapTryFindAndClearNX(bbitmap, tseq, n, pidx));
	if (n <= HMALLOC_BCHUNK_BITS)
		return (hmBBitmapTryFindAndClearNC(bbitmap, tseq, n, pidx));
	return (hmBBitmapTryFindAndClearN_(bbitmap, tseq, n, pidx));
}

#endif /* HMALLOC_BITMAP_H */
