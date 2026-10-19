/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file random.c
 * @brief Random number generation for hajmalloc.
 * @Created: 2026/10/06 12:37:13 by Moutig
 * @Updated: 2026/10/06 12:40:30 by Moutig
 *
 * The allocator uses randomness for two things:
 *
 *   - the per-page obfuscation keys (`keys[0]`, `keys[1]`) that
 *     encode the free-list pointers,
 *   - the default sampling rate for the profiler.
 *
 * Both are non-cryptographic: an attacker who can read a page
 * header already has read access to the process, and the keys
 * are only there to turn a simple corruption into a crash
 * instead of a silent pointer chase.
 *
 * The generator is xorshift128+. It has a period of 2^128-1,
 * passes the usual statistical tests, and is a few
 * instructions per call. State is per-thread (stored in the
 * calling thread's tld), so no lock and no atomic.
 *
 * Seeding
 * -------
 *
 * The seed is derived from, in order of preference:
 *
 *   1. hmallocPrimRandomBuf, which returns OS entropy in the
 *      native build (getrandom(2)) and false in the hosted
 *      build.
 *   2. A mix of the stack address, the TLS thread id, the
 *      tld pointer, and the timestamp counter, passed through
 *      splitmix64 to decorrelate the bits.
 *
 * The generator is never left in the all-zero state: if the
 * seed produces it, a fixed non-zero constant is XORed in.
 */

#include "include/hmBits.h"
#include "include/hmTypes.h"
#include "include/hmInternal.h"
#include "include/hmPrimTls.h"

#if defined(__x86_64__) || defined(__i386__)
# include <x86intrin.h>
#endif

/**
 * @brief Read the timestamp counter.
 *
 * On x86_64 and i386, uses the RDTSC instruction via the
 * compiler intrinsic. On aarch64, reads the virtual counter
 * via inline assembly. On other architectures, returns 0.
 *
 * The value is used only as entropy: it is not a clock.
 *
 * @return A monotonic, high-resolution counter value.
 */
static inline uint64_t	hmallocRdtsc(void)
{
#if defined(__x86_64__) || defined(__i386__)
	return ((uint64_t)__rdtsc());
#elif defined(__aarch64__) || defined(__arm__)
	uint64_t	v;

	__asm__ volatile ("mrs %0, cntvct_el0" : "=r" (v));
	return (v);
#else
	return (0);
#endif
}

/**
 * @brief One step of the SplitMix64 generator.
 *
 * Used to decorrelate a 64-bit seed before feeding it into
 * xorshift128+. SplitMix64 passes the standard statistical
 * tests and is much simpler than a full PRNG, which is all
 * we need here.
 *
 * @param state Pointer to the 64-bit state, updated in place.
 * @return The next 64-bit output.
 */
static inline uint64_t	hmallocSplitmix64(uint64_t *state)
{
	uint64_t	z;

	*state += 0x9E3779B97F4A7C15ULL;
	z = *state;
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
	return (z ^ (z >> 31));
}

/**
 * @brief Collect 128 bits of seed material.
 *
 * Tries OS entropy first. If the platform does not provide
 * it (hosted build), builds a seed from a mix of
 * process-level entropy sources: stack address, thread id,
 * the tld pointer, and the timestamp counter.
 *
 * @param tld   Thread-local data of the calling thread.
 * @param out0  First 64 bits of the seed.
 * @param out1  Second 64 bits of the seed.
 */
static void	hmallocSeed(struct s_hmallocTld *tld, uint64_t *out0, uint64_t *out1)
{
	unsigned char	buf[16];

	if (hmallocPrimRandomBuf(buf, sizeof(buf))) {
		/*
		 * OS entropy available. Copy the bytes out.
		 * memcpy is avoided on purpose: the allocator
		 * should not depend on the string module for
		 * sixteen bytes, and a manual loop lets the
		 * compiler fold it into two loads on most
		 * targets.
		 */
		uint64_t	a = 0;
		uint64_t	b = 0;

		for (int i = 0; i < 8; i++)
			a = (a << 8) | buf[i];
		for (int i = 8; i < 16; i++)
			b = (b << 8) | buf[i];
		*out0 = a;
		*out1 = b;
		return ;
	}

	/*
	 * No OS entropy. Build a seed from per-process and
	 * per-thread values. None of these is a strong source
	 * on its own, but the combination is different for
	 * every thread of every run.
	 */
	{
		uint64_t	state;
		uint64_t	tid;
		uint64_t	tsc;

		tid = (uint64_t)hmallocPrimThreadId();
		tsc = hmallocRdtsc();
		state = (uint64_t)(uintptr_t)tld
			  ^ ((uint64_t)(uintptr_t)&buf << 17)
			  ^ (tid << 33)
			  ^ (tsc << 3);

		*out0 = hmallocSplitmix64(&state);
		*out1 = hmallocSplitmix64(&state);
	}
}

void	_hmallocRandomInit(struct s_hmallocTld *tld)
{
	uint64_t	s0;
	uint64_t	s1;

	if (tld == NULL)
		return ;

	hmallocSeed(tld, &s0, &s1);

	/*
	 * xorshift128+ refuses the all-zero state. The
	 * probability of getting it by chance is 2^-128, but a
	 * buggy seed source could produce it, so we make sure.
	 */
	if (s0 == 0 && s1 == 0)
		s1 = 0x9E3779B97F4A7C15ULL;

	tld->rng[0] = s0;
	tld->rng[1] = s1;
}

size_t	_hmallocRandomNext(struct s_hmallocTld *tld)
{
	uint64_t	x;
	uint64_t	y;
	uint64_t	t;

	if (tld == NULL)
		return (0);

	x = tld->rng[0];
	y = tld->rng[1];

	tld->rng[0] = y;
	x ^= x << 23;
	tld->rng[1] = x ^ y ^ (x >> 17) ^ (y >> 26);

	t = tld->rng[1] + y;
	return ((size_t)t);
}
