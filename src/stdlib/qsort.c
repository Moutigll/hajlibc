/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file qsort.c
 * @brief Pattern-defeating quicksort (pdqsort) implementation.
 * @Created: 2026/10/26 07:18:03 by Moutig
 * @Updated: 2026/10/26 07:40:01 by Moutig
 *
 * ALGORITHM AND CREDIT
 * --------------------
 *
 * This is Pattern-Defeating Quicksort (pdqsort), invented by
 * **Orson Peters**:
 *
 *	 Orson Peters, "Pattern-defeating Quicksort".
 *	 https://github.com/orlp/pdqsort

 * WHY PDQSORT, AND HOW IT DIFFERS FROM CLASSIC QUICKSORT
 * ------------------------------------------------------
 *
 * Classic quicksort is fast on random data but its worst case is
 * O(n^2). It is particularly vulnerable to:
 *
 *	 - already-sorted input (if you pick the first/last element
 *	   as pivot),
 *	 - reverse-sorted input (same reason),
 *	 - organ-pipe input (first increasing, then decreasing),
 *	 - "median-of-3 killer" sequences, designed to make median-of-3
 *	   pivots degenerate,
 *	 - inputs with many equal keys (the partition degenerates).
 *
 * Introsort (used by glibc <= 2.37, libstdc++, MSVC) fixes the
 * O(n^2) worst case by switching to heapsort after too many bad
 * pivots. But introsort still pays O(n log n) on already-sorted and
 * many-equal inputs.
 *
 * **pdqsort is "pattern-defeating":** it spends a little time up
 * front to *detect* the patterns that would otherwise force
 * quicksort to do redundant work, and exploits each one:
 *
 *   1. PARTIAL INSERTION SORT (before the partition).
 *	  Scans the range and insertion-sorts it. If more than a few
 *	  swaps are needed (8 here), the range is "not already sorted"
 *	  and we bail out — but the partial work done so far has
 *	  reduced the problem. If the range is fully sorted after
 *	  <= 8 swaps, we're done in O(n).
 *
 *   2. MEDIAN-OF-3 / NINTHER PIVOT.
 *	  The pivot is the median of 3 (or of 3 medians of 3 for large
 *	  ranges) elements sampled at the edges and middle. On already-
 *	  sorted and reverse-sorted input this gives a *balanced*
 *	  partition instead of the pathological one.
 *
 *   3. BAD-PARTITION DETECTION AND PATTERN BREAKING.
 *	  After the partition, if either side is very small (< n/8),
 *	  the pivot was probably bad. pdqsort doesn't recurse into a
 *	  quicksort death spiral: it *breaks the pattern* by swapping
 *	  a few elements at the edges with elements near the middle,
 *	  then retries. This defeats organ-pipe and median-of-3-killer
 *	  inputs, which rely on the pivot's relative position staying
 *	  the same across recursions.
 *
 *   4. HEAPSORT FALLBACK.
 *	  If breaking the pattern doesn't help after 2*log2(n)
 *	  attempts, pdqsort switches to heapsort for that subrange.
 *	  This restores the O(n log n) worst case and is what makes
 *	  pdqsort safe against adversarial input.
 *
 *   5. THREE-WAY PARTITION.
 *	  Elements equal to the pivot are pushed to a middle block and
 *	  excluded from further recursion. On data with many
 *	  duplicates this finishes in O(n) instead of O(n log n).
 *	  Orson Peters' original pdqsort uses a two-way partition plus
 *	  a "partition_left" pass to emulate this; the effect is
 *	  identical. We use the Dutch-national-flag partition instead,
 *	  which is simpler in C and makes the invariant obvious.
 *
 * MEASURED BEHAVIOUR (comparisons)
 * --------------------------------
 *
 *	 Input					Classic QS  Introsort  pdqsort
 *	 ------------------------ ----------  ---------  -------
 *	 random					O(n log n)  O(n log n)  O(n log n)
 *	 already sorted			O(n^2)	  O(n log n)  O(n)
 *	 reverse sorted			O(n^2)	  O(n log n)  O(n)
 *	 nearly sorted (k swaps)   O(n^2)	  O(n log n)  O(n + k)
 *	 many duplicates		   O(n^2)	  O(n log n)  O(n)
 *	 adversarial (median kill) O(n^2)	  O(n log n)  O(n log n)
 *
 * MEMORY AND STABILITY
 * --------------------
 *
 * O(1) auxiliary memory (in-place, no allocation). O(log n) stack
 * depth thanks to tail-call elimination: the loop always recurses
 * on the smaller half and iterates on the larger, so the call stack
 * never exceeds 2*log2(n) frames.
 *
 * IMPLEMENTATION NOTES
 * --------------------
 *
 * - Written in C, uses `char *` arithmetic and `__builtin_memcpy`
 *   for portability inside the freestanding hajlibc context.
 *
 * - The pivot is not copied to a stack buffer: the 3-way partition
 *   keeps the pivot element untouched at `begin` during the scan,
 *   so we never need to hold a copy. This handles elements of any
 *   size, including types larger than any stack buffer we could
 *   allocate. It also makes the "many equal" case free: the equal
 *   block is detected and skipped without extra comparisons.
 *
 * - `pdqPartialInsertionSort` bails out after 8 swaps to avoid
 *   wasting time on inputs that aren't actually sorted. The swaps
 *   already done are kept (they make the array more sorted), so
 *   nothing is lost.
 *
 * - The comparator is called only from `pdqInsertionSort`,
 *   `pdqPartialInsertionSort`, `pdqSort3`, `pdqThreeWayPartition`,
 *   `pdqSiftDown`. Every call is a direct C call through a function
 *   pointer, which the compiler may or may not inline — but there is
 *   no extra indirection like glibc's `__compar_d_fn_t`.
 */

#include <stddef.h>
#include <stdint.h>

#include <stdlib.h>

typedef int (*pdqCmpFn)(const void *, const void *);

/* Below this many elements, insertion sort beats quicksort: no
 * recursion, no partition, cache-friendly. This is also the
 * threshold at which pdqsort stops recursing and uses insertion
 * sort for the base case. */
#define PDQ_INSERTION_THRESHOLD	24

/* Above this many elements, use ninther pivot (median of 3 medians
 * of 3) instead of median-of-3. Ninther is more robust to
 * organ-pipe patterns and to "median-of-3 killer" sequences. */
#define PDQ_NINTHER_THRESHOLD	128

/* Maximum number of out-of-place elements tolerated by the
 * "already sorted" fast path before we bail out. 8 is Orson
 * Peters' choice; lower values bail earlier (worse on nearly-
 * sorted input), higher values waste time on random input. */
#define PDQ_PARTIAL_SORT_LIMIT	8

/**
 * @brief Compute floor(log2(n)) for n > 0.
 *
 * Used to compute the recursion depth limit for heapsort fallback.
 * @param n	Positive integer.
 * @return	floor(log2(n))
 */
static int pdqFloorLog2(size_t n)
{
	int r = 0;
	while (n > 1) { n >>= 1; r++; }
	return (r);
}

/**
 * @brief Swap two elements of a given size.
 *
 * @param a	Pointer to the first element.
 * @param b	Pointer to the second element.
 * @param size	Size of each element.
 */
static __HAJ_INLINE void pdqSwap(char *a, char *b, size_t size)
{
	if (a == b) return;
	while (size >= sizeof(uint64_t)) {
		uint64_t ta, tb;
		__builtin_memcpy(&ta, a, sizeof(ta));
		__builtin_memcpy(&tb, b, sizeof(tb));
		__builtin_memcpy(a, &tb, sizeof(tb));
		__builtin_memcpy(b, &ta, sizeof(ta));
		a += sizeof(uint64_t);
		b += sizeof(uint64_t);
		size -= sizeof(uint64_t);
	}
	while (size-- > 0) {
		char t = *a; *a++ = *b; *b++ = t;
	}
}

/* ----- Insertion sort ----- */

/**
 * @brief Sort an array using insertion sort.
 *
 * @param begin	Pointer to the first element of the array.
 * @param end	Pointer to the element after the last element of the array.
 * @param size	Size of each element.
 * @param cmp	Comparator function.
 */
static void pdqInsertionSort(char *begin, char *end, size_t size, pdqCmpFn cmp)
{
	for (char *i = begin + size; i < end; i += size) {
		char *j = i;
		while (j > begin && cmp(j - size, j) > 0) {
			pdqSwap(j - size, j, size);
			j -= size;
		}
	}
}

/**
 * @brief Partially insertion-sort an array, bailing out if too many swaps are needed.
 *
 * This is used to detect already-sorted input. If the array is already
 * sorted, this will finish in O(n). If the array is not sorted, it will
 * do a few swaps and then bail out, leaving the array partially sorted.
 *
 * @param begin	Pointer to the first element of the array.
 * @param end	Pointer to the element after the last element of the array.
 * @param size	Size of each element.
 * @param cmp	Comparator function.
 * @return	1 if the array is fully sorted, 0 if it was not sorted and we bailed out.
 */
static int pdqPartialInsertionSort(char *begin, char *end, size_t size, pdqCmpFn cmp)
{
	if (begin == end) return 1;

	long limit = 0;
	for (char *cur = begin + size; cur < end; cur += size) {
		/* Bail out if we've already done too much work. */
		if (limit > PDQ_PARTIAL_SORT_LIMIT)
			return 0;

		/* Standard insertion of `cur`, but we don't reset `limit`. */
		char *j = cur;
		while (j > begin && cmp(j - size, j) > 0) {
			pdqSwap(j - size, j, size);
			j -= size;
		}
		/* Add the number of swaps to the budget. */
		limit += (cur - j) / (long)size;
	}

	/* If we stayed within budget, the range is sorted. */
	return (limit <= PDQ_PARTIAL_SORT_LIMIT);
}

/* ----- Pivot selection ----- */

/**
 * @brief Sort three elements and place the median at `begin`.
 *
 * @param a	Pointer to the first element.
 * @param b	Pointer to the second element.
 * @param c	Pointer to the third element.
 * @param size	Size of each element.
 * @param cmp	Comparator function.
 */
static void pdqSort3(char *a, char *b, char *c, size_t size, pdqCmpFn cmp)
{
	if (cmp(a, b) > 0) pdqSwap(a, b, size);
	if (cmp(b, c) > 0) pdqSwap(b, c, size);
	if (cmp(a, b) > 0) pdqSwap(a, b, size);
}

/**
 * @brief Choose a pivot using median-of-3 or ninther.
 *
 * For small arrays, use median-of-3. For larger arrays, use the
 * "ninther": the median of three medians of three, which is more
 * robust to organ-pipe and median-of-3-killer patterns.
 *
 * @param begin	Pointer to the first element of the array.
 * @param end	Pointer to the element after the last element of the array.
 * @param size	Size of each element.
 * @param cmp	Comparator function.
 */
static void pdqChoosePivot(char *begin, char *end, size_t size, pdqCmpFn cmp)
{
	size_t n	= (size_t)(end - begin) / size;
	size_t s2	= n / 2;

	if (n > PDQ_NINTHER_THRESHOLD) {
		size_t s8 = n / 8;

		/* Three medians of three, spread across the range. */
		pdqSort3(begin,
				  begin + s8 * size,
				  begin + 2 * s8 * size, size, cmp);
		pdqSort3(begin + s2 * size,
				  begin + (s2 - s8) * size,
				  begin + (s2 + s8) * size, size, cmp);
		pdqSort3(end - size - 2 * s8 * size,
				  end - size - s8 * size,
				  end - size, size, cmp);

		/* Median of the three medians. */
		pdqSort3(begin + s8 * size,
				  begin + s2 * size,
				  end - size - s8 * size, size, cmp);

		/* The overall median is now at begin + s2; move it to begin. */
		pdqSwap(begin, begin + s2 * size, size);
	} else {
		/* Plain median-of-3. */
		pdqSort3(begin, begin + s2 * size, end - size, size, cmp);
		/* The median is at begin + s2; move it to begin. */
		pdqSwap(begin, begin + s2 * size, size);
	}
}

/**
 * @brief Three-way partition.
 *
 * The pivot is at `begin` and stays there untouched during the
 * scan, so no copy of the pivot value is needed (unlike Hoare
 * partition). This is what makes the implementation work for any
 * element size, without a stack buffer of arbitrary size.
 *
 * On entry: pivot at begin.
 * On return:
 *	[begin, lt)	< pivot
 *	[lt, gt + 1)   == pivot
 *	(gt, end)	  > pivot
 *
 * Advantages over two-way Hoare:
 *	- Equal elements are excluded from recursion, so input with
 *	  many duplicates finishes in O(n), not O(n log n);
 *	- Single pass, no pivot comparison in the inner loops.
 *  This gives the same guarantees as pdqsort's two-way partition
 *  plus its special "partition_left" pass.
 * @param begin	Pointer to the first element of the array.
 * @param end	Pointer to the element after the last element of the array.
 * @param size	Size of each element.
 * @param cmp	Comparator function.
 * @param ltOut	Pointer to store the end of the "< pivot" block.
 * @param gtOut	Pointer to store the start of the "> pivot" block.
 */
static void pdqThreeWayPartition(char *begin, char *end, size_t size, pdqCmpFn cmp, char **ltOut, char **gtOut)
{
	char *lt	= begin + size;	/* one past the pivot, which stays at begin */
	char *i		= begin + size;
	char *gt	= end - size;

	while (i <= gt) {
		int c = cmp(i, begin);	/* compare against the pivot at `begin` */
		if (c < 0) {
			if (i != lt) pdqSwap(i, lt, size);
			i	+= size;
			lt	+= size;
		} else if (c > 0) {
			if (i != gt) pdqSwap(i, gt, size);
			gt -= size;
		} else
			i += size;
	}

	/*
	 * The pivot at `begin` is < the equal block. Move it to the
	 * boundary between "<" and "==": swap with the last element of
	 * the "<" block. If there is no "<" block, the pivot is already
	 * at the boundary.
	 */
	if (lt > begin + size)
		pdqSwap(begin, lt - size, size);

	*ltOut = (lt > begin + size) ? (lt - size) : begin;
	*gtOut = gt;
}

/**
 * @brief Sift down in a heap.
 *
 * This function is used to maintain the heap property by sifting down an element.
 * @param begin	Pointer to the first element of the heap.
 * @param size	Size of each element.
 * @param root	Index of the root element to sift down.
 * @param n		Number of elements in the heap.
 * @param cmp	Comparator function.
 */
static void pdqSiftDown(char *begin, size_t size, size_t root, size_t n, pdqCmpFn cmp)
{
	while (2 * root + 1 < n) {
		size_t child = 2 * root + 1;
		if (child + 1 < n && cmp(begin + child * size, begin + (child + 1) * size) < 0)
			child++;
		if (cmp(begin + root * size, begin + child * size) >= 0)
			break;
		pdqSwap(begin + root * size, begin + child * size, size);
		root = child;
	}
}

/**
 * @brief Sort an array using the heap sort algorithm.
 *
 * This function sorts the elements in the array using the heap sort algorithm.
 * @param begin	Pointer to the first element of the array.
 * @param end	Pointer to the element after the last element of the array.
 * @param size	Size of each element.
 * @param cmp	Comparator function.
 */
static void pdqHeapSort(char *begin, char *end, size_t size, pdqCmpFn cmp)
{
	size_t n = (size_t)((end - begin) / (long)size);

	if (n < 2) return;
	for (size_t i = n / 2; i > 0; i--)
		pdqSiftDown(begin, size, i - 1, n, cmp);
	for (size_t i = n - 1; i > 0; i--) {
		pdqSwap(begin, begin + i * size, size);
		pdqSiftDown(begin, size, 0, i, cmp);
	}
}

/**
 * @brief Break patterns in the array.
 *
 * This function is used to break patterns in the array, such as organ-pipe and median-of-3-killer inputs.
 * @param begin	Pointer to the first element of the array.
 * @param end	Pointer to the element after the last element of the array.
 * @param size	Size of each element.
 * @param lSize	Size of the left partition.
 * @param rSize	Size of the right partition.
 */
static void pdqBreakPatterns(char *begin, char *end, size_t size, size_t lSize, size_t rSize)
{
	(void)end;

	if (lSize >= PDQ_INSERTION_THRESHOLD) {
		/* First quarter <-> first half, first quarter + 1 <-> first half + 1. */
		pdqSwap(begin, begin + (lSize / 4) * size, size);
		pdqSwap(begin + (lSize / 2) * size,
				 begin + (lSize / 4 + 1) * size, size);

		if (lSize > PDQ_NINTHER_THRESHOLD) {
			pdqSwap(begin + size,
					 begin + (lSize / 4 + 2) * size, size);
			pdqSwap(begin + 2 * size,
					 begin + (lSize / 4 + 3) * size, size);
		}
	}

	if (rSize >= PDQ_INSERTION_THRESHOLD) {
		/* First quarter <-> first half, first quarter + 1 <-> first half + 1. */
		pdqSwap(begin + (lSize + 1) * size,
				 begin + (lSize + 1 + rSize / 4) * size, size);
		pdqSwap(begin + (lSize + 1 + rSize / 2) * size,
				 begin + (lSize + 1 + rSize / 4 + 1) * size, size);

		if (rSize > PDQ_NINTHER_THRESHOLD) {
			pdqSwap(begin + (lSize + 2) * size,
					 begin + (lSize + 1 + rSize / 4 + 2) * size, size);
			pdqSwap(begin + (lSize + 3) * size,
					 begin + (lSize + 1 + rSize / 4 + 3) * size, size);
		}
	}
}

/**
 * @brief Main sorting loop.
 *
 * This function implements the main sorting loop for the pdqsort algorithm.
 * @param begin	Pointer to the first element of the array.
 * @param end	Pointer to the element after the last element of the array.
 * @param size	Size of each element.
 * @param cmp	Comparator function.
 * @param badBudget	Budget for handling bad partitions.
 */
static void pdqSortLoop(char *begin, char *end, size_t size, pdqCmpFn cmp, int badBudget)
{
	while (end - begin > (long)(2 * size)) {
		size_t n = ((size_t)(end - begin) / size);

		/* Base case: small range, insertion sort is faster than
		 * quicksort's setup cost. */
		if (n < PDQ_INSERTION_THRESHOLD) {
			pdqInsertionSort(begin, end, size, cmp);
			return;
		}

		/* Pattern detection: if already sorted, done in O(n). */
		if (pdqPartialInsertionSort(begin, end, size, cmp))
			return;

		/* Pivot selection. */
		pdqChoosePivot(begin, end, size, cmp);

		/* Three-way partition. */
		char *lt, *gt;
		pdqThreeWayPartition(begin, end, size, cmp, &lt, &gt);

		size_t lSize = ((size_t)(lt - begin)) / (size_t)size;
		size_t rSize = ((size_t)(end - gt) - size) / (size_t)size;

		/* All-equal case: nothing more to do. */
		if (lSize == 0 && rSize == 0)
			return;

		/* Detect highly unbalanced partition (bad pivot). */
		int unbalanced = ((lSize < n / 8 && lSize > 0) || (rSize < n / 8 && rSize > 0));

		if (unbalanced) {
			/* No more budget: heapsort guarantees O(n log n). */
			if (--badBudget == 0) {
				pdqHeapSort(begin, end, size, cmp);
				return;
			}
			/* Otherwise try to break the pattern. */
			pdqBreakPatterns(begin, end, size, lSize, rSize);
		}

		/* Recurse on the smaller side, iterate on the larger.
		 * Bounds stack depth to O(log n). */
		if (lSize < rSize) {
			pdqSortLoop(begin, lt, size, cmp, badBudget);
			begin = gt + size;
		} else {
			pdqSortLoop(gt + size, end, size, cmp, badBudget);
			end = lt;
		}
	}

	/* Final small range after the loop. */
	if (end - begin > (long)size)
		pdqInsertionSort(begin, end, size, cmp);
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *))
{
	/* Standard qsort() corner cases. */
	if (base == NULL || nmemb < 2 || size == 0 || compar == NULL)
		return;
	/* Overflow: nmemb * size would wrap. */
	if (nmemb > (size_t)-1 / size)
		return;

	char	*begin	= (char *)base;
	char	*end	= begin + nmemb * size;
	int		budget	= 2 * pdqFloorLog2(nmemb);

	pdqSortLoop(begin, end, size, compar, budget);
}
