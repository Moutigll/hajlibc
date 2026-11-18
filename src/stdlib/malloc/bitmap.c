/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file bitmap.c
 * @brief Two-level and binned bitmaps for tracking abandoned pages and free slices.
 * @Created: 2026/11/16 17:23:28 by Moutig
 * @Updated: 2026/11/16 17:31:45 by Moutig
 *
 * This file implements the two-level and binned bitmaps used by the hajmalloc allocator
 * to track abandoned pages and free slices. The bitmaps provide efficient
 * mechanisms for managing memory allocation and deallocation,
 * allowing the allocator to quickly find available memory regions and reclaim abandoned pages.
 */


#include "include/hmPrivate.h"
#include "include/hmBitmap.h"

#include <string.h>

/* ----- bfields ----- */

static inline size_t	hmBFieldCtz(t_hmBField x)	{ return (hmallocCtz(x)); }
static inline size_t	hmBFieldClz(t_hmBField x)	{ return (hmallocClz(x)); }

static inline size_t	hmBFieldPopcount(t_hmBField x)
{
#if defined(__GNUC__) || defined(__clang__)
	return ((size_t)__builtin_popcountll((unsigned long long)x));
#else
	size_t n = 0;

	while (x != 0) {
		x &= x - 1;
		n++;
	}
	return (n);
#endif
}

static inline t_hmBField	hmBFieldClearLeastBit(t_hmBField x)
{
	return (x & (x - 1));
}

/* Index of the least significant set bit; false if x == 0. */
static inline bool	hmBFieldFindLeastBit(t_hmBField x, size_t *idx)
{
	if (x == 0)
		return (false);
	*idx = hmBFieldCtz(x);
	return (true);
}

/* Index of the most significant set bit; false if x == 0. */
static inline bool	hmBFieldFindHighestBit(t_hmBField x, size_t *idx)
{
	if (x == 0)
		return (false);
	*idx = HMALLOC_BFIELD_BITS - 1 - hmBFieldClz(x);
	return (true);
}

static inline t_hmBField	hmBFieldAllSet(void)
{
	return (~(t_hmBField)0);
}

/* `bitCount` bits set, shifted left by `shiftl`. */
static inline t_hmBField	hmBFieldMask(size_t bitCount, size_t shiftl)
{
	t_hmBField mask0 = (bitCount < HMALLOC_BFIELD_BITS)
		? (((t_hmBField)1 << bitCount) - 1)
		: hmBFieldAllSet();

	return (mask0 << shiftl);
}

/*
 * Cycle iteration over the set bits of a bfield: first [start, cycle),
 * then [0, start) and [cycle, BITS). Threads start at different spots
 * (start = tseq % cycle) to reduce contention, while the `cycle` limits
 * the first pass to the part that was accessed before (better reuse).
 */
#define HM_BFIELD_ITERATE(bfield, start, cycle, idxName, SUF) { \
	const t_hmBField	_cycleMask##SUF = hmBFieldMask((cycle) - (start), (start)); \
	size_t				_bcount##SUF = hmBFieldPopcount(bfield); \
	t_hmBField			_b##SUF = (bfield) & _cycleMask##SUF; \
	while (_bcount##SUF > 0) { \
		_bcount##SUF--; \
		if (_b##SUF == 0) \
			_b##SUF = (bfield) & ~_cycleMask##SUF; \
		(void)hmBFieldFindLeastBit(_b##SUF, &(idxName)); \
		_b##SUF = hmBFieldClearLeastBit(_b##SUF); \
		{

#define HM_BFIELD_ITERATE_END(SUF) \
		} \
	} \
}

#define HM_BFIELD_CYCLE_ITERATE(bfield, tseq, cycle, idxName, SUF) { \
	const size_t _start##SUF = (uint32_t)(tseq) % (uint32_t)(cycle); \
	HM_BFIELD_ITERATE(bfield, _start##SUF, cycle, idxName, SUF)

#define HM_BFIELD_CYCLE_ITERATE_END(SUF) \
	HM_BFIELD_ITERATE_END(SUF); \
}

/* ----- Atomic bfield operations ----- */

/* Set a bit; true if it transitioned 0 -> 1. */
static inline bool	hmBFieldAtomicSet(_Atomic(t_hmBField) *b, size_t idx)
{
	const t_hmBField	mask = hmBFieldMask(1, idx);
	const t_hmBField	old = atomic_fetch_or_explicit(b, mask, memory_order_acq_rel);

	return ((old & mask) == 0);
}

/* Clear a bit; true if it transitioned 1 -> 0. */
static inline bool	hmBFieldAtomicClear(_Atomic(t_hmBField) *b, size_t idx,
										bool *allClear)
{
	const t_hmBField	mask = hmBFieldMask(1, idx);
	const t_hmBField	old = atomic_fetch_and_explicit(b, ~mask, memory_order_acq_rel);

	if (allClear != NULL)
		*allClear = ((old & ~mask) == 0);
	return ((old & mask) == mask);
}

/**
 * @brief Clear a bit in a bfield only if it was previously set, using atomic operations.
 *
 * This function clears a bit in a bfield only if it was previously set, using atomic operations.
 * It returns true if the bit was cleared, false if it was already clear.
 *
 * @param b Pointer to the atomic bfield.
 * @param idx The index of the bit to clear.
 * @return True if the bit was cleared, false if it was already clear.
 */
static inline void	hmBFieldAtomicClearOnceSet(_Atomic(t_hmBField) *b, size_t idx)
{
	const t_hmBField	mask = hmBFieldMask(1, idx);
	t_hmBField			old = atomic_load_explicit(b, memory_order_relaxed);

	do {
		if (hmUnlikely((old & mask) == 0)) {
			old = atomic_load_explicit(b, memory_order_acquire);
			while ((old & mask) == 0) {
				hmallocPrimThreadYield();
				old = atomic_load_explicit(b, memory_order_acquire);
			}
		}
	} while (!atomic_compare_exchange_weak_explicit(b, &old, old & ~mask,
				memory_order_acq_rel, memory_order_relaxed));
}

/* Set a mask; true if the mask bits went from all 0 to all 1. */
static inline bool	hmBFieldAtomicSetMask(_Atomic(t_hmBField) *b, t_hmBField mask,
										  size_t *alreadySet)
{
	const t_hmBField old = atomic_fetch_or_explicit(b, mask, memory_order_acq_rel);

	if (alreadySet != NULL)
		*alreadySet = hmBFieldPopcount(old & mask);
	return ((old & mask) == 0);
}

/* Clear a mask; true if the mask bits went from all 1 to all 0. */
static inline bool	hmBFieldAtomicClearMask(_Atomic(t_hmBField) *b, t_hmBField mask,
											bool *allClear)
{
	const t_hmBField old = atomic_fetch_and_explicit(b, ~mask, memory_order_acq_rel);

	if (allClear != NULL)
		*allClear = ((old & ~mask) == 0);
	return ((old & mask) == mask);
}

static inline bool	hmBFieldAtomicSetX(_Atomic(t_hmBField) *b, size_t *alreadySet)
{
	const t_hmBField old = atomic_exchange_explicit(b, hmBFieldAllSet(),
		memory_order_release);

	if (alreadySet != NULL)
		*alreadySet = hmBFieldPopcount(old);
	return (old == 0);
}

/**
 * @brief Optimistically clear a mask with one atomic `and`.
 * @param b Pointer to the atomic bfield.
 * @param mask The mask of bits to clear.
 * @param previous Pointer to store the previous value of the bfield.
 * @param didTempClearBits Pointer to store whether any bits were temporarily cleared.
 * @return True if all mask bits were set and cleared, false otherwise.
 */
static inline bool	hmBFieldAtomicTryClearMaskOptimistic(_Atomic(t_hmBField) *b,
		t_hmBField mask, t_hmBField *previous, bool *didTempClearBits)
{
	t_hmBField old = atomic_fetch_and_explicit(b, ~mask, memory_order_acq_rel);

	if (previous != NULL)
		*previous = old;
	if (hmLikely((old & mask) == mask))
		return (true);
	if ((old & mask) != 0) {
		atomic_fetch_or_explicit(b, old & mask, memory_order_acq_rel);
		if (didTempClearBits != NULL)
			*didTempClearBits = true;
	}
	return (false);
}

static inline bool	hmBFieldAtomicTryClear8Optimistic(_Atomic(t_hmBField) *b,
		size_t idx, t_hmBField *previous, bool *didTempClearBits)
{
	const t_hmBField mask = ((t_hmBField)0xFF) << idx;

	return (hmBFieldAtomicTryClearMaskOptimistic(b, mask, previous, didTempClearBits));
}

/* Clear a mask only if all its bits are currently set (checked first). */
static inline bool	hmBFieldAtomicTryClearMask(_Atomic(t_hmBField) *b,
		t_hmBField mask, bool *allClear, bool *didTempClearBits)
{
	t_hmBField	old = atomic_load_explicit(b, memory_order_relaxed);
	bool		cleared = false;

	if ((old & mask) == mask)
		cleared = hmBFieldAtomicTryClearMaskOptimistic(b, mask, &old, didTempClearBits);
	if (allClear != NULL)
		*allClear = ((cleared ? (old & ~mask) : old) == 0);
	return (cleared);
}

/* Clear a whole bfield only if all its bits are set. */
static inline bool	hmBFieldAtomicTryClearX(_Atomic(t_hmBField) *b, bool *allClear)
{
	t_hmBField	old = atomic_load_explicit(b, memory_order_relaxed);
	bool		cleared = false;

	if (old == hmBFieldAllSet())
		cleared = atomic_compare_exchange_strong_explicit(b, &old, (t_hmBField)0,
			memory_order_acq_rel, memory_order_relaxed);
	if (allClear != NULL)
		*allClear = (cleared ? true : (old == 0));
	return (cleared);
}

static inline bool	hmBFieldAtomicIsSetMask(const _Atomic(t_hmBField) *b, t_hmBField mask)
{
	const t_hmBField x = atomic_load_explicit((_Atomic(t_hmBField) *)b,
		memory_order_acquire);

	return ((x & mask) == mask);
}

static inline bool	hmBFieldAtomicIsClearMask(const _Atomic(t_hmBField) *b, t_hmBField mask)
{
	const t_hmBField x = atomic_load_explicit((_Atomic(t_hmBField) *)b,
		memory_order_acquire);

	return ((x & mask) == 0);
}

static inline bool	hmBFieldAtomicIsXsetMask(bool set, const _Atomic(t_hmBField) *b,
											 t_hmBField mask)
{
	return (set ? hmBFieldAtomicIsSetMask(b, mask) : hmBFieldAtomicIsClearMask(b, mask));
}

/* ----- Chunks ----- */

static bool	hmBChunkSet(t_hmBChunk *chunk, size_t cidx, size_t *alreadySet)
{
	const size_t	i = cidx / HMALLOC_BFIELD_BITS;
	const size_t	idx = cidx % HMALLOC_BFIELD_BITS;
	const bool		wasClear = hmBFieldAtomicSet(&chunk->bfields[i], idx);

	if (alreadySet != NULL)
		*alreadySet = (wasClear ? 0 : 1);
	return (wasClear);
}

static inline bool	hmBChunkSetNX(t_hmBChunk *chunk, size_t cidx, size_t n, size_t *alreadySet)
{
	const size_t	i = cidx / HMALLOC_BFIELD_BITS;
	const size_t	idx = cidx % HMALLOC_BFIELD_BITS;
	size_t			alreadySet1;
	size_t			alreadySet2;
	size_t			m;
	bool			allSet1;
	bool			allSet2;

	if (hmLikely(idx + n <= HMALLOC_BFIELD_BITS))
		return (hmBFieldAtomicSetMask(&chunk->bfields[i], hmBFieldMask(n, idx), alreadySet));
	m = HMALLOC_BFIELD_BITS - idx;
	allSet1 = hmBFieldAtomicSetMask(&chunk->bfields[i], hmBFieldMask(m, idx), &alreadySet1);
	allSet2 = hmBFieldAtomicSetMask(&chunk->bfields[i + 1], hmBFieldMask(n - m, 0), &alreadySet2);
	if (alreadySet != NULL)
		*alreadySet = alreadySet1 + alreadySet2;
	return (allSet1 && allSet2);
}

/* Set or clear `n` bits at `cidx` (may cover several bfields). */
static bool	hmBChunkXsetNC(bool set, t_hmBChunk *chunk, size_t cidx, size_t n,
						   size_t *palreadySet, bool *pmaybeAllClear)
{
	bool	allTransition = true;
	bool	maybeAllClear = true;
	size_t	totalAlreadySet = 0;
	size_t	idx = cidx % HMALLOC_BFIELD_BITS;
	size_t	field = cidx / HMALLOC_BFIELD_BITS;

	while (n > 0) {
		size_t				m = HMALLOC_BFIELD_BITS - idx;
		t_hmBField			mask;
		size_t				alreadySet = 0;
		bool				allClear = false;
		bool				transition;

		if (m > n)
			m = n;
		mask = hmBFieldMask(m, idx);
		transition = set
			? hmBFieldAtomicSetMask(&chunk->bfields[field], mask, &alreadySet)
			: hmBFieldAtomicClearMask(&chunk->bfields[field], mask, &allClear);
		allTransition = allTransition && transition;
		totalAlreadySet += alreadySet;
		maybeAllClear = maybeAllClear && allClear;
		field++;
		idx = 0;
		n -= m;
	}
	if (palreadySet != NULL)
		*palreadySet = totalAlreadySet;
	if (pmaybeAllClear != NULL)
		*pmaybeAllClear = maybeAllClear;
	return (allTransition);
}

static bool	hmBChunkSetN(t_hmBChunk *chunk, size_t cidx, size_t n, size_t *alreadySet)
{
	if (n == 1)
		return (hmBChunkSet(chunk, cidx, alreadySet));
	if (n <= HMALLOC_BFIELD_BITS)
		return (hmBChunkSetNX(chunk, cidx, n, alreadySet));
	return (hmBChunkXsetNC(true, chunk, cidx, n, alreadySet, NULL));
}

static bool	hmBChunkClear(t_hmBChunk *chunk, size_t cidx, bool *allClear)
{
	const size_t i = cidx / HMALLOC_BFIELD_BITS;
	const size_t idx = cidx % HMALLOC_BFIELD_BITS;

	return (hmBFieldAtomicClear(&chunk->bfields[i], idx, allClear));
}

static bool	hmBChunkClearN(t_hmBChunk *chunk, size_t cidx, size_t n,
						   bool *maybeAllClear)
{
	if (n == 1)
		return (hmBChunkClear(chunk, cidx, maybeAllClear));
	return (hmBChunkXsetNC(false, chunk, cidx, n, NULL, maybeAllClear));
}

static bool	hmBChunkIsXsetNC(bool set, const t_hmBChunk *chunk, size_t field,
							 size_t idx, size_t n)
{
	while (n > 0) {
		size_t m = HMALLOC_BFIELD_BITS - idx;

		if (m > n)
			m = n;
		if (!hmBFieldAtomicIsXsetMask(set, &chunk->bfields[field], hmBFieldMask(m, idx)))
			return (false);
		field++;
		idx = 0;
		n -= m;
	}
	return (true);
}

static bool	hmBChunkIsXsetN(bool set, const t_hmBChunk *chunk, size_t cidx, size_t n)
{
	const size_t i = cidx / HMALLOC_BFIELD_BITS;
	const size_t idx = cidx % HMALLOC_BFIELD_BITS;

	if (n == 0)
		return (true);
	if (idx + n <= HMALLOC_BFIELD_BITS)
		return (hmBFieldAtomicIsXsetMask(set, &chunk->bfields[i], hmBFieldMask(n, idx)));
	return (hmBChunkIsXsetNC(set, chunk, i, idx, n));
}

/**
 * @brief Optimistically clear a mask with one atomic `and`.
 *
 * This function attempts to clear a mask in a bfield optimistically using one atomic `and` operation.
 * It returns true if all mask bits were set and cleared, false otherwise.
 *
 * @param b Pointer to the atomic bfield.
 * @param mask The mask of bits to clear.
 * @param previous Pointer to store the previous value of the bfield.
 * @param didTempClearBits Pointer to store whether any bits were temporarily cleared.
 * @return True if all mask bits were set and cleared, false otherwise.
 */
static inline bool	hmBChunkTryClearNX(t_hmBChunk *chunk, size_t cidx, size_t n,
									   bool *pmaybeAllClear, bool *didTempClearBits)
{
	const size_t	i = cidx / HMALLOC_BFIELD_BITS;
	const size_t	idx = cidx % HMALLOC_BFIELD_BITS;
	size_t			m;
	bool			field1IsClear;
	bool			field2IsClear;

	if (hmLikely(idx + n <= HMALLOC_BFIELD_BITS))
		return (hmBFieldAtomicTryClearMask(&chunk->bfields[i], hmBFieldMask(n, idx),
			pmaybeAllClear, didTempClearBits));
	m = HMALLOC_BFIELD_BITS - idx;
	if (!hmBFieldAtomicTryClearMask(&chunk->bfields[i], hmBFieldMask(m, idx),
			&field1IsClear, didTempClearBits))
		return (false);
	if (!hmBFieldAtomicTryClearMask(&chunk->bfields[i + 1], hmBFieldMask(n - m, 0),
			&field2IsClear, didTempClearBits)) {
		hmBFieldAtomicSetMask(&chunk->bfields[i], hmBFieldMask(m, idx), NULL);
		if (didTempClearBits != NULL)
			*didTempClearBits = true;
		return (false);
	}
	if (pmaybeAllClear != NULL)
		*pmaybeAllClear = field1IsClear && field2IsClear;
	return (true);
}

/**
 * @brief Clear multiple bits in a bchunk only if they were previously set.
 *
 * This function clears multiple bits in a bchunk only if they were previously set.
 * It returns true if all specified bits were cleared, false if any of them were already clear.
 *
 * @param chunk Pointer to the bchunk.
 * @param cidx The starting index of the bits to clear.
 * @param n The number of bits to clear.
 * @param pmaybeAllClear Pointer to a variable that will store whether all bits are now clear.
 * @param didTempClearBits Pointer to a variable that will store whether any bits were temporarily cleared.
 * @return True if all specified bits were cleared, false otherwise.
 */
static bool	hmBChunkTryClearNC(t_hmBChunk *chunk, size_t cidx, size_t n,
		bool *pmaybeAllClear, bool *didTempClearBits)
{
	const size_t	startIdx = cidx % HMALLOC_BFIELD_BITS;
	const size_t	startField = cidx / HMALLOC_BFIELD_BITS;
	size_t			field = startField;
	size_t			m = HMALLOC_BFIELD_BITS - startIdx;
	t_hmBField		maskStart;
	bool			maybeAllClear;
	bool			fieldIsClear;

	if (pmaybeAllClear != NULL)
		*pmaybeAllClear = true;
	if (n == 0)
		return (true);
	if (m > n)
		m = n;
	maskStart = hmBFieldMask(m, startIdx);
	if (!hmBFieldAtomicTryClearMask(&chunk->bfields[field], maskStart,
			&maybeAllClear, didTempClearBits))
		return (false);
	n -= m;
	while (n >= HMALLOC_BFIELD_BITS) {
		field++;
		if (!hmBFieldAtomicTryClearX(&chunk->bfields[field], &fieldIsClear))
			goto restore;
		maybeAllClear = maybeAllClear && fieldIsClear;
		n -= HMALLOC_BFIELD_BITS;
	}
	if (n > 0) {
		field++;
		if (!hmBFieldAtomicTryClearMask(&chunk->bfields[field], hmBFieldMask(n, 0),
				&fieldIsClear, didTempClearBits))
			goto restore;
		maybeAllClear = maybeAllClear && fieldIsClear;
	}
	if (pmaybeAllClear != NULL)
		*pmaybeAllClear = maybeAllClear;
	return (true);

restore:
	if (didTempClearBits != NULL)
		*didTempClearBits = true;
	while (field > startField) {
		field--;
		if (field == startField)
			hmBFieldAtomicSetMask(&chunk->bfields[field], maskStart, NULL);
		else
			hmBFieldAtomicSetX(&chunk->bfields[field], NULL);
	}
	return (false);
}

static bool	hmBChunkTryClearN(t_hmBChunk *chunk, size_t cidx, size_t n,
		bool *maybeAllClear, bool *didTempClearBits)
{
	if (n <= HMALLOC_BFIELD_BITS)
		return (hmBChunkTryClearNX(chunk, cidx, n, maybeAllClear, didTempClearBits));
	return (hmBChunkTryClearNC(chunk, cidx, n, maybeAllClear, didTempClearBits));
}

/* ----- Chunk find-and-clear ----- */

/**
 * @brief Try to find and clear a single set bit in a bchunk at a specific chunk index.
 *
 * This function attempts to find and clear a single set bit in a bchunk at the specified chunk index.
 * It uses atomic operations to ensure thread safety. If a set bit is found and cleared, the index of
 * the cleared bit is stored in `pidx`.
 *
 * @param chunk Pointer to the bchunk.
 * @param chunkIdx The index of the chunk to search for a set bit.
 * @param pidx Pointer to store the index of the cleared bit if found.
 * @return True if a set bit was found and cleared, false otherwise.
 */
static inline bool	hmBChunkTryFindAndClearAt(t_hmBChunk *chunk, size_t chunkIdx, size_t *pidx)
{
	_Atomic(t_hmBField)	*bfield = &chunk->bfields[chunkIdx];
	t_hmBField			b = atomic_load_explicit(bfield, memory_order_relaxed);
	int					tries = 0;

	if (b == 0)
		return (false);
	do {
		const t_hmBField	mask = (b & (~b + 1));	/* least significant set bit */
		size_t				bitIdx = 0;

		b = atomic_fetch_and_explicit(bfield, ~mask, memory_order_acq_rel);
		if (hmLikely((b & mask) == mask)) {
			(void)hmBFieldFindLeastBit(mask, &bitIdx);
			*pidx = (chunkIdx * HMALLOC_BFIELD_BITS) + bitIdx;
			return (true);
		}
	} while (b != 0 && ++tries <= 4);	/* limit tries to reduce contention */
	return (false);
}

/* Find a single set bit and clear it. */
static bool	hmBChunkTryFindAndClear(t_hmBChunk *chunk, size_t *pidx)
{
	for (size_t i = 0; i < HMALLOC_BCHUNK_FIELDS; i++) {
		if (hmBChunkTryFindAndClearAt(chunk, i, pidx))
			return (true);
	}
	return (false);
}

static bool	hmBChunkTryFindAndClear1(t_hmBChunk *chunk, size_t n, size_t *pidx,
									 bool *didTempClearBits)
{
	(void)n;
	(void)didTempClearBits;
	return (hmBChunkTryFindAndClear(chunk, pidx));
}

/**
 * @brief Try to find and clear a byte-aligned run of 8 set bits in a bchunk at a specific chunk index.
 *
 * This function attempts to find and clear a byte-aligned run of 8 set bits in a bchunk at the specified chunk index.
 * It uses atomic operations to ensure thread safety. If a run of 8 set bits is found and cleared, the index of
 * the cleared bits is stored in `pidx`.
 *
 * @param chunk Pointer to the bchunk.
 * @param chunkIdx The index of the chunk to search for a run of 8 set bits.
 * @param pidx Pointer to store the index of the cleared bits if found.
 * @param didTempClearBits Pointer to store whether any bits were temporarily cleared.
 * @return True if a run of 8 set bits was found and cleared, false otherwise.
 */
static inline bool	hmBChunkTryFindAndClear8At(t_hmBChunk *chunk, size_t chunkIdx,
											   size_t *pidx, bool *didTempClearBits)
{
	_Atomic(t_hmBField)	*bfield = &chunk->bfields[chunkIdx];
	t_hmBField			b = atomic_load_explicit(bfield, memory_order_relaxed);
	int					tries = 0;

	if (b == 0)
		return (false);
	do {
		/* low bit of each byte that is 0xFF */
		const t_hmBField hasSet8 =
			((~b - HMALLOC_BFIELD_LO_BIT8) & (b & HMALLOC_BFIELD_HI_BIT8)) >> 7;
		size_t bitIdx;

		if (!hmBFieldFindLeastBit(hasSet8, &bitIdx))
			return (false);
		if (hmLikely(hmBFieldAtomicTryClear8Optimistic(bfield, bitIdx, &b, didTempClearBits))) {
			*pidx = (chunkIdx * HMALLOC_BFIELD_BITS) + bitIdx;
			return (true);
		}
	} while (b != 0 && ++tries <= 4);
	return (false);
}

/* Find a byte-aligned run of 8 set bits and clear it. */
static bool	hmBChunkTryFindAndClear_8(t_hmBChunk *chunk, size_t n, size_t *pidx,
									  bool *didTempClearBits)
{
	(void)n;
	for (size_t i = 0; i < HMALLOC_BCHUNK_FIELDS; i++) {
		if (hmBChunkTryFindAndClear8At(chunk, i, pidx, didTempClearBits))
			return (true);
	}
	return (false);
}

/**
 * @brief Try to find and clear a run of `n` contiguous set bits across bfields.
 *
 * This function attempts to find and clear a run of `n` contiguous set bits across the bfields of a bchunk.
 * It uses atomic operations to ensure thread safety. If a run of `n` set bits is found and cleared, the index of
 * the cleared bits is stored in `pidx`.
 *
 * @param chunk Pointer to the bchunk.
 * @param n The number of contiguous set bits to find and clear.
 * @param pidx Pointer to store the index of the cleared bits if found.
 * @param didTempClearBits Pointer to store whether any bits were temporarily cleared.
 * @return True if a run of `n` set bits was found and cleared, false otherwise.
 */
static bool	hmBChunkTryFindAndClearNX(t_hmBChunk *chunk, size_t n, size_t *pidx, bool *didTempClearBits)
{
	const t_hmBField mask = hmBFieldMask(n, 0);

	if (n == 0 || n > HMALLOC_BFIELD_BITS)
		return (false);
	for (size_t i = 0; i < HMALLOC_BCHUNK_FIELDS; i++) {
		t_hmBField	b0 = atomic_load_explicit(&chunk->bfields[i], memory_order_relaxed);
		t_hmBField	b = b0;
		size_t		idx;

		while (hmBFieldFindLeastBit(b, &idx)) {
			t_hmBField bmask;

			if (idx + n > HMALLOC_BFIELD_BITS)
				break;
			bmask = mask << idx;
			if ((b & bmask) == bmask) {
				if (hmLikely(hmBFieldAtomicTryClearMaskOptimistic(&chunk->bfields[i],
						bmask, &b0, didTempClearBits))) {
					*pidx = (i * HMALLOC_BFIELD_BITS) + idx;
					return (true);
				}
				b = b0;
			}
			else
				b = b & (b + ((t_hmBField)1 << idx));	/* skip this run of ones */
		}
		/* A run that crosses into the next bfield? */
		if (b != 0 && i < HMALLOC_BCHUNK_FIELDS - 1) {
			const size_t post = hmBFieldClz(~b);

			if (post > 0) {
				const size_t pre = hmBFieldCtz(~atomic_load_explicit(
					&chunk->bfields[i + 1], memory_order_relaxed));

				if (post + pre >= n) {
					const size_t cidx = (i * HMALLOC_BFIELD_BITS)
						+ (HMALLOC_BFIELD_BITS - post);

					if (hmBChunkTryClearNX(chunk, cidx, n, NULL, didTempClearBits)) {
						*pidx = cidx;
						return (true);
					}
				}
			}
		}
	}
	return (false);
}

/**
 * @brief Try to find and clear a run of `n` contiguous set bits across bfields.
 *
 * This function attempts to find and clear a run of `n` contiguous set bits across the bfields of a bchunk.
 * It uses atomic operations to ensure thread safety. If a run of `n` set bits is found and cleared, the index of
 * the cleared bits is stored in `pidx`.
 *
 * @param chunk Pointer to the bchunk.
 * @param n The number of contiguous set bits to find and clear.
 * @param pidx Pointer to store the index of the cleared bits if found.
 * @param didTempClearBits Pointer to store whether any bits were temporarily cleared.
 * @return True if a run of `n` set bits was found and cleared, false otherwise.
 */
static bool	hmBChunkTryFindAndClearNC(t_hmBChunk *chunk, size_t n, size_t *pidx, bool *didTempClearBits)
{
	size_t skipCount;
	size_t cidx;

	if (n == 0 || n > HMALLOC_BCHUNK_BITS)
		return (false);
	skipCount = (n - 1) / HMALLOC_BFIELD_BITS;
	for (size_t i = 0; i < HMALLOC_BCHUNK_FIELDS - skipCount; i++) {
		size_t		m = n;
		t_hmBField	b = atomic_load_explicit(&chunk->bfields[i], memory_order_relaxed);
		size_t		ones = hmBFieldClz(~b);

		cidx = (i * HMALLOC_BFIELD_BITS) + (HMALLOC_BFIELD_BITS - ones);
		if (ones >= m)
			m = 0;
		else if (ones > 0) {
			size_t j = 1;

			m -= ones;
			while (i + j < HMALLOC_BCHUNK_FIELDS) {
				b = atomic_load_explicit(&chunk->bfields[i + j], memory_order_relaxed);
				ones = hmBFieldCtz(~b);
				if (ones >= m) {
					m = 0;
					break;
				}
				else if (ones == HMALLOC_BFIELD_BITS) {
					j++;
					m -= HMALLOC_BFIELD_BITS;
				}
				else {
					i = i + j - 1;	/* rescan only the last field (with clz) */
					break;
				}
			}
		}
		if (m == 0 && hmBChunkTryClearN(chunk, cidx, n, NULL, didTempClearBits)) {
			*pidx = cidx;
			return (true);
		}
	}
	return (false);
}

static void	hmBChunkClearOnceSet(t_hmBChunk *chunk, size_t cidx)
{
	const size_t i = cidx / HMALLOC_BFIELD_BITS;
	const size_t idx = cidx % HMALLOC_BFIELD_BITS;

	hmBFieldAtomicClearOnceSet(&chunk->bfields[i], idx);
}

static bool	hmBChunkAllAreClearRelaxed(t_hmBChunk *chunk)
{
	for (size_t i = 0; i < HMALLOC_BCHUNK_FIELDS; i++) {
		if (atomic_load_explicit(&chunk->bfields[i], memory_order_relaxed) != 0)
			return (false);
	}
	return (true);
}

static bool	hmBChunkAllAreSetRelaxed(t_hmBChunk *chunk)
{
	for (size_t i = 0; i < HMALLOC_BCHUNK_FIELDS; i++) {
		if (~atomic_load_explicit(&chunk->bfields[i], memory_order_relaxed) != 0)
			return (false);
	}
	return (true);
}

static size_t	hmBChunkPopcount(t_hmBChunk *chunk)
{
	size_t popcount = 0;

	for (size_t i = 0; i < HMALLOC_BCHUNK_FIELDS; i++)
		popcount += hmBFieldPopcount(atomic_load_explicit(&chunk->bfields[i],
			memory_order_relaxed));
	return (popcount);
}

/**
 * @brief Unsafely set `n` bits starting from a given index in a bchunk and its chunkmap.
 *
 * This function sets `n` bits starting from the specified index in a bchunk and its corresponding chunkmap.
 * It does not perform any bounds checking or synchronization, so it should only be used in contexts where
 * it is safe to do so (e.g., during initialization).
 *
 * @param chunks Pointer to the array of bchunks.
 * @param cmap Pointer to the chunkmap.
 * @param idx The starting index of the bits to set.
 * @param n The number of bits to set.
 */
static void	hmBChunksUnsafeSetN(t_hmBChunk *chunks, t_hmBChunkmap *cmap, size_t idx, size_t n)
{
	size_t			chunkIdx = idx / HMALLOC_BCHUNK_BITS;
	const size_t	cidx = idx % HMALLOC_BCHUNK_BITS;
	const size_t	ccount = ((cidx + n - 1) / HMALLOC_BCHUNK_BITS) + 1;
	size_t			m = HMALLOC_BCHUNK_BITS - cidx;
	size_t			midChunks;

	if (n == 0)
		return;
	hmBChunkSetN(cmap, chunkIdx, ccount, NULL);
	if (m > n)
		m = n;
	hmBChunkSetN(&chunks[chunkIdx], cidx, m, NULL);
	chunkIdx++;
	n -= m;
	midChunks = n / HMALLOC_BCHUNK_BITS;
	if (midChunks > 0) {
		memset(&chunks[chunkIdx], 0xFF, midChunks * HMALLOC_BCHUNK_SIZE);
		chunkIdx += midChunks;
		n -= midChunks * HMALLOC_BCHUNK_BITS;
	}
	if (n > 0)
		hmBChunkSetN(&chunks[chunkIdx], 0, n, NULL);
}

static inline size_t	hmDivideUp(size_t x, size_t d)
{
	return ((x + d - 1) / d);
}

/* ----- Bitmap ----- */

static inline size_t	hmBitmapChunkCount(t_hmBitmap *bitmap)
{
	return (atomic_load_explicit(&bitmap->chunkCount, memory_order_relaxed));
}

static inline size_t	hmBitmapMaxBits(t_hmBitmap *bitmap)
{
	return (hmBitmapChunkCount(bitmap) * HMALLOC_BCHUNK_BITS);
}

static void	hmBitmapChunkmapSet(t_hmBitmap *bitmap, size_t chunkIdx)
{
	hmBChunkSet(&bitmap->chunkmap, chunkIdx, NULL);
}

/*
 * Clear the chunkmap bit of an all-clear chunk. A concurrent set may have
 * happened between the test and the clear, so test again and restore.
 */
static bool	hmBitmapChunkmapTryClear(t_hmBitmap *bitmap, size_t chunkIdx)
{
	if (!hmBChunkAllAreClearRelaxed(&bitmap->chunks[chunkIdx]))
		return (false);
	hmBChunkClear(&bitmap->chunkmap, chunkIdx, NULL);
	if (!hmBChunkAllAreClearRelaxed(&bitmap->chunks[chunkIdx])) {
		hmBChunkSet(&bitmap->chunkmap, chunkIdx, NULL);
		return (false);
	}
	return (true);
}

void	hmBitmapInit(t_hmBitmap *bitmap, size_t bitCount)
{
	size_t chunkCount = hmDivideUp(bitCount, HMALLOC_BCHUNK_BITS);

	if (chunkCount > HMALLOC_ARENA_CHUNK_COUNT)
		chunkCount = HMALLOC_ARENA_CHUNK_COUNT;
	memset(bitmap, 0, sizeof(*bitmap));
	atomic_store_explicit(&bitmap->chunkCount, chunkCount, memory_order_release);
}

bool	hmBitmapSetN(t_hmBitmap *bitmap, size_t idx, size_t n, size_t *palreadySet)
{
	const size_t	maxBits = hmBitmapMaxBits(bitmap);
	size_t			chunkIdx;
	size_t			cidx;
	bool			wereAllClear = true;
	size_t			alreadySet = 0;

	if (idx + n > maxBits) {
		if (idx >= maxBits)
			return (false);
		n = maxBits - idx;
	}
	chunkIdx = idx / HMALLOC_BCHUNK_BITS;
	cidx = idx % HMALLOC_BCHUNK_BITS;
	while (n > 0) {
		const size_t	m = (cidx + n > HMALLOC_BCHUNK_BITS) ? HMALLOC_BCHUNK_BITS - cidx : n;
		size_t			chunkAlreadySet = 0;

		wereAllClear = hmBChunkSetN(&bitmap->chunks[chunkIdx], cidx, m, &chunkAlreadySet)
			&& wereAllClear;
		alreadySet += chunkAlreadySet;
		hmBitmapChunkmapSet(bitmap, chunkIdx);	/* set afterwards */
		n -= m;
		cidx = 0;
		chunkIdx++;
	}
	if (palreadySet != NULL)
		*palreadySet = alreadySet;
	return (wereAllClear);
}

bool	hmBitmapClearN(t_hmBitmap *bitmap, size_t idx, size_t n)
{
	const size_t	maxBits = hmBitmapMaxBits(bitmap);
	size_t			chunkIdx;
	size_t			cidx;
	bool			wereAllSet = true;

	if (idx + n > maxBits) {
		if (idx >= maxBits)
			return (false);
		n = maxBits - idx;
	}
	chunkIdx = idx / HMALLOC_BCHUNK_BITS;
	cidx = idx % HMALLOC_BCHUNK_BITS;
	while (n > 0) {
		const size_t	m = (cidx + n > HMALLOC_BCHUNK_BITS) ? HMALLOC_BCHUNK_BITS - cidx : n;
		bool			maybeAllClear = false;

		wereAllSet = hmBChunkClearN(&bitmap->chunks[chunkIdx], cidx, m, &maybeAllClear)
			&& wereAllSet;
		if (maybeAllClear)
			hmBitmapChunkmapTryClear(bitmap, chunkIdx);
		n -= m;
		cidx = 0;
		chunkIdx++;
	}
	return (wereAllSet);
}

bool	hmBitmapIsXsetN(bool set, t_hmBitmap *bitmap, size_t idx, size_t n)
{
	const size_t	maxBits = hmBitmapMaxBits(bitmap);
	size_t			chunkIdx;
	size_t			cidx;
	bool			xset = true;

	if (idx + n > maxBits) {
		if (idx >= maxBits)
			return (false);
		n = maxBits - idx;
	}
	chunkIdx = idx / HMALLOC_BCHUNK_BITS;
	cidx = idx % HMALLOC_BCHUNK_BITS;
	while (n > 0 && xset) {
		const size_t m = (cidx + n > HMALLOC_BCHUNK_BITS) ? HMALLOC_BCHUNK_BITS - cidx : n;

		xset = hmBChunkIsXsetN(set, &bitmap->chunks[chunkIdx], cidx, m) && xset;
		n -= m;
		cidx = 0;
		chunkIdx++;
	}
	return (xset);
}

size_t	hmBitmapPopcount(t_hmBitmap *bitmap)
{
	const size_t	chunkCount = hmBitmapChunkCount(bitmap);
	size_t			popcount = 0;

	for (size_t i = 0; i < chunkCount; i++)
		popcount += hmBChunkPopcount(&bitmap->chunks[i]);
	return (popcount);
}

void	hmBitmapClearOnceSet(t_hmBitmap *bitmap, size_t idx)
{
	const size_t chunkIdx = idx / HMALLOC_BCHUNK_BITS;
	const size_t cidx = idx % HMALLOC_BCHUNK_BITS;

	if (chunkIdx >= hmBitmapChunkCount(bitmap))
		return;
	hmBChunkClearOnceSet(&bitmap->chunks[chunkIdx], cidx);
}

/**
 * @brief Try to find and claim a set bit in a bitmap at a specific chunk index.
 *
 * This function attempts to find and claim a set bit in a bitmap at the specified chunk index. It uses
 * atomic operations to ensure thread safety. If a set bit is found and claimed, the index of the claimed
 * bit is stored in `pidx`. The `claim` function is called to determine if the bit can be claimed.
 *
 * @param bitmap Pointer to the bitmap.
 * @param chunkIdx The index of the chunk to search for a set bit.
 * @param pidx Pointer to store the index of the claimed bit if found.
 * @param claim Function pointer to the claim function that determines if the bit can be claimed.
 * @param arg Argument to pass to the claim function.
 * @return True if a set bit was found and claimed, false otherwise.
 */
static bool	hmBitmapTryFindAndClaimVisit(t_hmBitmap *bitmap, size_t chunkIdx,
		size_t *pidx, t_hmClaimFun *claim, void *arg)
{
	size_t cidx;

	if (hmLikely(hmBChunkTryFindAndClear(&bitmap->chunks[chunkIdx], &cidx))) {
		const size_t	sliceIdx = (chunkIdx * HMALLOC_BCHUNK_BITS) + cidx;
		bool			keepSet = true;

		if ((*claim)(sliceIdx, arg, &keepSet)) {
			*pidx = sliceIdx;
			return (true);
		}
		if (keepSet) {
			hmBChunkSet(&bitmap->chunks[chunkIdx], cidx, NULL);
			hmBitmapChunkmapSet(bitmap, chunkIdx);
		}
	}
	else {
		/* The chunkmap is conservative: we may only now see the chunk is empty. */
		hmBitmapChunkmapTryClear(bitmap, chunkIdx);
	}
	return (false);
}

bool	hmBitmapTryFindAndClaim(t_hmBitmap *bitmap, size_t tseq, size_t *pidx,
								t_hmClaimFun *claim, void *arg)
{
	const size_t chunkmapMax = hmDivideUp(hmBitmapChunkCount(bitmap), HMALLOC_BFIELD_BITS);

	for (size_t i = 0; i < chunkmapMax; i++) {
		const t_hmBField	cmapEntry = atomic_load_explicit(&bitmap->chunkmap.bfields[i],
			memory_order_relaxed);
		size_t				hi;

		if (hmBFieldFindHighestBit(cmapEntry, &hi)) {
			size_t eidx = 0;

			/* tseq reduced to 8 start points to limit memory spread. */
			HM_BFIELD_CYCLE_ITERATE(cmapEntry, tseq % 8, hi + 1, eidx, Y)
			{
				const size_t chunkIdx = i * HMALLOC_BFIELD_BITS + eidx;

				if (hmBitmapTryFindAndClaimVisit(bitmap, chunkIdx, pidx, claim, arg))
					return (true);
			}
			HM_BFIELD_CYCLE_ITERATE_END(Y);
		}
	}
	return (false);
}

/* ----- Binned bitmap ----- */

static inline size_t	hmBBitmapChunkCount(t_hmBBitmap *bbitmap)
{
	return (atomic_load_explicit(&bbitmap->chunkCount, memory_order_relaxed));
}

static inline size_t	hmBBitmapMaxBits(t_hmBBitmap *bbitmap)
{
	return (hmBBitmapChunkCount(bbitmap) * HMALLOC_BCHUNK_BITS);
}

/* Assign a size class to a chunk (NONE clears every class bit). */
static void	hmBBitmapSetChunkBin(t_hmBBitmap *bbitmap, size_t chunkIdx, t_hmChunkBin bin)
{
	for (int ibin = HMALLOC_CBIN_SMALL; ibin < HMALLOC_CBIN_NONE; ibin++) {
		if ((t_hmChunkBin)ibin == bin) {
			if (hmBChunkSet(&bbitmap->chunkmapBins[ibin], chunkIdx, NULL))
				hmStatsSubprocIncrease(chunkBins[ibin], 1);
		}
		else if (hmBChunkClear(&bbitmap->chunkmapBins[ibin], chunkIdx, NULL))
			hmStatsSubprocDecrease(chunkBins[ibin], 1);
	}
}

/* Track the highest chunk index that was accessed. */
static void	hmBBitmapChunkmapSetMax(t_hmBBitmap *bbitmap, size_t chunkIdx)
{
	size_t oldMax = atomic_load_explicit(&bbitmap->chunkMaxAccessed, memory_order_relaxed);

	if (hmUnlikely(chunkIdx > oldMax))
		atomic_compare_exchange_strong_explicit(&bbitmap->chunkMaxAccessed, &oldMax,
			chunkIdx, memory_order_relaxed, memory_order_relaxed);
}

static void	hmBBitmapChunkmapSet(t_hmBBitmap *bbitmap, size_t chunkIdx, bool checkAllSet)
{
	/* All slices free again: the chunk returns to the NONE class. */
	if (checkAllSet && hmBChunkAllAreSetRelaxed(&bbitmap->chunks[chunkIdx]))
		hmBBitmapSetChunkBin(bbitmap, chunkIdx, HMALLOC_CBIN_NONE);
	hmBChunkSet(&bbitmap->chunkmap, chunkIdx, NULL);
	hmBBitmapChunkmapSetMax(bbitmap, chunkIdx);
}

static bool	hmBBitmapChunkmapTryClear(t_hmBBitmap *bbitmap, size_t chunkIdx)
{
	if (!hmBChunkAllAreClearRelaxed(&bbitmap->chunks[chunkIdx]))
		return (false);
	hmBChunkClear(&bbitmap->chunkmap, chunkIdx, NULL);
	if (!hmBChunkAllAreClearRelaxed(&bbitmap->chunks[chunkIdx])) {
		hmBChunkSet(&bbitmap->chunkmap, chunkIdx, NULL);
		return (false);
	}
	hmBBitmapChunkmapSetMax(bbitmap, chunkIdx);
	return (true);
}

void	hmBBitmapInit(t_hmBBitmap *bbitmap, size_t bitCount)
{
	size_t chunkCount = hmDivideUp(bitCount, HMALLOC_BCHUNK_BITS);

	if (chunkCount > HMALLOC_ARENA_CHUNK_COUNT)
		chunkCount = HMALLOC_ARENA_CHUNK_COUNT;
	memset(bbitmap, 0, sizeof(*bbitmap));
	atomic_store_explicit(&bbitmap->chunkCount, chunkCount, memory_order_release);
}

void	hmBBitmapUnsafeSetN(t_hmBBitmap *bbitmap, size_t idx, size_t n)
{
	hmBChunksUnsafeSetN(&bbitmap->chunks[0], &bbitmap->chunkmap, idx, n);
}

bool	hmBBitmapSetN(t_hmBBitmap *bbitmap, size_t idx, size_t n)
{
	const size_t	maxBits = hmBBitmapMaxBits(bbitmap);
	size_t			chunkIdx;
	size_t			cidx;
	bool			wereAllClear = true;

	if (idx + n > maxBits) {
		if (idx >= maxBits)
			return (false);
		n = maxBits - idx;
	}
	chunkIdx = idx / HMALLOC_BCHUNK_BITS;
	cidx = idx % HMALLOC_BCHUNK_BITS;
	while (n > 0) {
		const size_t m = (cidx + n > HMALLOC_BCHUNK_BITS) ? HMALLOC_BCHUNK_BITS - cidx : n;

		wereAllClear = hmBChunkSetN(&bbitmap->chunks[chunkIdx], cidx, m, NULL)
			&& wereAllClear;
		hmBBitmapChunkmapSet(bbitmap, chunkIdx, true);	/* set afterwards */
		n -= m;
		cidx = 0;
		chunkIdx++;
	}
	return (wereAllClear);
}

bool	hmBBitmapTryClearNC(t_hmBBitmap *bbitmap, size_t idx, size_t n)
{
	const size_t	chunkIdx = idx / HMALLOC_BCHUNK_BITS;
	const size_t	cidx = idx % HMALLOC_BCHUNK_BITS;
	bool			maybeAllClear = false;
	bool			didTempClearBits = false;
	bool			cleared;

	if (cidx + n > HMALLOC_BCHUNK_BITS)
		return (false);
	cleared = hmBChunkTryClearN(&bbitmap->chunks[chunkIdx], cidx, n,
		&maybeAllClear, &didTempClearBits);
	if (cleared && maybeAllClear)
		hmBBitmapChunkmapTryClear(bbitmap, chunkIdx);
	else if (didTempClearBits)
		hmBBitmapChunkmapSet(bbitmap, chunkIdx, false);
	return (cleared);
}

bool	hmBBitmapIsXsetN(bool set, t_hmBBitmap *bbitmap, size_t idx, size_t n)
{
	const size_t	maxBits = hmBBitmapMaxBits(bbitmap);
	size_t			chunkIdx;
	size_t			cidx;
	bool			xset = true;

	if (idx + n > maxBits) {
		if (idx >= maxBits)
			return (false);
		n = maxBits - idx;
	}
	chunkIdx = idx / HMALLOC_BCHUNK_BITS;
	cidx = idx % HMALLOC_BCHUNK_BITS;
	while (n > 0 && xset) {
		const size_t m = (cidx + n > HMALLOC_BCHUNK_BITS) ? HMALLOC_BCHUNK_BITS - cidx : n;

		xset = hmBChunkIsXsetN(set, &bbitmap->chunks[chunkIdx], cidx, m) && xset;
		n -= m;
		cidx = 0;
		chunkIdx++;
	}
	return (xset);
}

typedef bool	(t_hmBChunkTryFindAndClearFun)(t_hmBChunk *chunk, size_t n,
					size_t *idx, bool *didTempClearBits);

/**
 * @brief Try to find and clear a run of `n` contiguous set bits in a binned bitmap.
 *
 * This function attempts to find and clear a run of `n` contiguous set bits in a binned bitmap. It uses
 * atomic operations to ensure thread safety. If a run of `n` set bits is found and cleared, the index of the
 * cleared bits is stored in `pidx`. The `onFind` function is called to determine if the bits can be cleared.
 *
 * @param bbitmap Pointer to the binned bitmap.
 * @param tseq The starting point for the search sequence.
 * @param n The number of contiguous set bits to find and clear.
 * @param pidx Pointer to store the index of the cleared bits if found.
 * @param onFind Function pointer to the onFind function that determines if the bits can be cleared.
 * @return True if a run of `n` set bits was found and cleared, false otherwise.
 */
static bool	hmBBitmapTryFindAndClearGeneric(t_hmBBitmap		*bbitmap,	size_t	tseq,
											size_t			n,			size_t	*pidx,
											t_hmBChunkTryFindAndClearFun *onFind)
{
	const size_t		cmapMaxCount = hmDivideUp(hmBBitmapChunkCount(bbitmap),
		HMALLOC_BFIELD_BITS);
	const size_t		chunkAcc = atomic_load_explicit(&bbitmap->chunkMaxAccessed,
		memory_order_relaxed);
	const size_t		cmapAcc = chunkAcc / HMALLOC_BFIELD_BITS;
	const size_t		cmapAccBits = 1 + (chunkAcc % HMALLOC_BFIELD_BITS);
	const t_hmBField	cmapMask = hmBFieldMask(cmapMaxCount, 0);
	const size_t		cmapCycle = cmapAcc + 1;
	const t_hmChunkBin	bbin = hmChunkBinOf(n);
	size_t				cmapIdx = 0;

	HM_BFIELD_CYCLE_ITERATE(cmapMask, tseq, cmapCycle, cmapIdx, X)
	{
		const t_hmBField	cmapEntry = atomic_load_explicit(
			&bbitmap->chunkmap.bfields[cmapIdx], memory_order_relaxed);
		const size_t		cmapEntryCycle = (cmapIdx != cmapAcc)
			? HMALLOC_BFIELD_BITS : cmapAccBits;
		t_hmBField			cmapBins[HMALLOC_CBIN_COUNT] = { 0 };

		if (cmapEntry == 0)
			continue;

		/* Per size-class masks; NONE are chunks without any class bit. */
		cmapBins[HMALLOC_CBIN_NONE] = cmapEntry;
		for (int ibin = HMALLOC_CBIN_SMALL; ibin < HMALLOC_CBIN_NONE; ibin++) {
			const t_hmBField cmapBin = atomic_load_explicit(
				&bbitmap->chunkmapBins[ibin].bfields[cmapIdx], memory_order_relaxed);

			cmapBins[ibin] = cmapBin & cmapEntry;
			cmapBins[HMALLOC_CBIN_NONE] &= ~cmapBin;
		}

		/* From SMALL up to `bbin`, then skip to NONE. */
		for (int ibin = HMALLOC_CBIN_SMALL; ibin <= HMALLOC_CBIN_NONE;
			 ibin = ((t_hmChunkBin)ibin == bbin) ? HMALLOC_CBIN_NONE : ibin + 1) {
			const t_hmBField	cmapBin = cmapBins[ibin];
			size_t				eidx = 0;

			HM_BFIELD_CYCLE_ITERATE(cmapBin, tseq, cmapEntryCycle, eidx, Y)
			{
				const size_t	chunkIdx = cmapIdx * HMALLOC_BFIELD_BITS + eidx;
				t_hmBChunk		*chunk = &bbitmap->chunks[chunkIdx];
				size_t			cidx;
				bool			didTempClearBits = false;
				const bool		found = (*onFind)(chunk, n, &cidx, &didTempClearBits);

				/* A concurrent find_and_claim may have cleared the chunkmap bit. */
				if (didTempClearBits)
					hmBBitmapChunkmapSet(bbitmap, chunkIdx, false);
				if (found) {
					/* Only the first block decides the class of a free chunk. */
					if (cidx == 0 && (t_hmChunkBin)ibin == HMALLOC_CBIN_NONE)
						hmBBitmapSetChunkBin(bbitmap, chunkIdx, bbin);
					*pidx = (chunkIdx * HMALLOC_BCHUNK_BITS) + cidx;
					return (true);
				}
				if (!didTempClearBits)
					hmBBitmapChunkmapTryClear(bbitmap, chunkIdx);
			}
			HM_BFIELD_CYCLE_ITERATE_END(Y);
			if ((t_hmChunkBin)ibin == HMALLOC_CBIN_NONE)
				break;
		}
	}
	HM_BFIELD_CYCLE_ITERATE_END(X);
	return (false);
}

bool	hmBBitmapTryFindAndClear(t_hmBBitmap *bbitmap, size_t tseq, size_t *pidx)
{
	return (hmBBitmapTryFindAndClearGeneric(bbitmap, tseq, 1, pidx,
		&hmBChunkTryFindAndClear1));
}

bool	hmBBitmapTryFindAndClear8(t_hmBBitmap *bbitmap, size_t tseq, size_t *pidx)
{
	return (hmBBitmapTryFindAndClearGeneric(bbitmap, tseq, 8, pidx,
		&hmBChunkTryFindAndClear_8));
}

bool	hmBBitmapTryFindAndClearNX(t_hmBBitmap *bbitmap, size_t tseq, size_t n, size_t *pidx)
{
	return (hmBBitmapTryFindAndClearGeneric(bbitmap, tseq, n, pidx,
		&hmBChunkTryFindAndClearNX));
}

bool	hmBBitmapTryFindAndClearNC(t_hmBBitmap *bbitmap, size_t tseq, size_t n, size_t *pidx)
{
	return (hmBBitmapTryFindAndClearGeneric(bbitmap, tseq, n, pidx,
		&hmBChunkTryFindAndClearNC));
}

/**
 * @brief Try to clear `n` bits starting from a given chunk index in a binned bitmap.
 *
 * This function attempts to clear `n` bits starting from the specified chunk index in a binned bitmap.
 * It uses atomic operations to ensure thread safety. If the bits are successfully cleared, the function
 * returns true. If any of the bits cannot be cleared, it rolls back any changes made and returns false.
 *
 * @param bbitmap Pointer to the binned bitmap.
 * @param chunkIdx The starting index of the chunk to clear bits from.
 * @param n The number of bits to clear.
 * @return True if the bits were successfully cleared, false otherwise.
 */
static bool	hmBBitmapTryClearN_(t_hmBBitmap *bbitmap, size_t chunkIdx, size_t n)
{
	size_t m = n;
	size_t count = 0;

	while (m > 0) {
		t_hmBChunk	*chunk = &bbitmap->chunks[chunkIdx + count];
		bool		didTempClearBits = false;

		if (!hmBChunkTryClearN(chunk, 0, (m > HMALLOC_BCHUNK_BITS) ? HMALLOC_BCHUNK_BITS : m,
				NULL, &didTempClearBits)) {
			if (didTempClearBits)
				hmBBitmapChunkmapSet(bbitmap, chunkIdx + count, false);
			goto rollback;
		}
		m = (m <= HMALLOC_BCHUNK_BITS) ? 0 : m - HMALLOC_BCHUNK_BITS;
		count++;
	}
	return (true);

rollback:
	/* Only the chunks we fully cleared need to be reset. */
	while (count > 0) {
		count--;
		hmBChunkSetN(&bbitmap->chunks[chunkIdx + count], 0, HMALLOC_BCHUNK_BITS, NULL);
		hmBBitmapChunkmapSet(bbitmap, chunkIdx + count, false);
	}
	return (false);
}

bool	hmBBitmapTryFindAndClearN_(t_hmBBitmap *bbitmap, size_t tseq, size_t n, size_t *pidx)
{
	const size_t	chunkMax = hmBBitmapChunkCount(bbitmap);
	const size_t	chunkReq = hmDivideUp(n, HMALLOC_BCHUNK_BITS);
	size_t			chunkIdx = 0;

	(void)tseq;
	if (chunkMax < chunkReq)
		return (false);
	while (chunkIdx <= chunkMax - chunkReq) {
		size_t count = 0;

		do {
			if (!hmBChunkAllAreSetRelaxed(&bbitmap->chunks[chunkIdx + count]))
				break;
			count++;
		} while (count < chunkReq);

		if (count == chunkReq) {
			if (hmBBitmapTryClearN_(bbitmap, chunkIdx, n)) {
				*pidx = chunkIdx * HMALLOC_BCHUNK_BITS;
				for (size_t i = 0; i < count; i++)
					hmBBitmapSetChunkBin(bbitmap, chunkIdx + i, HMALLOC_CBIN_HUGE);
				return (true);
			}
			/* Contended: retry, still skipping the first chunk for progress. */
			count = 0;
		}
		chunkIdx += count + 1;
	}
	return (false);
}
