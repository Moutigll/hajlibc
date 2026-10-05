/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file stack_chk_guard.c
 * @brief Definition of __stack_chk_guard.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/30 09:20:15 by Moutig
 *

 * WHAT IS THE STACK GUARD ?
 *
 * When compiled with -fstack-protector (or -fstack-protector-strong,
 * or -fstack-protector-all), the compiler inserts a "canary"
 * value on the stack, between the local variables and the saved
 * return address:
 *
 *   +-------------------------+  <- high address
 *   | saved return address    |
 *   +-------------------------+
 *   | saved rbp / frame ptr   |
 *   +-------------------------+
 *   | __stack_chk_guard value |  <- the canary
 *   +-------------------------+
 *   | local variables         |
 *   +-------------------------+  <- rsp
 *
 * Before returning from the function, the compiler compares the
 * value on the stack with __stack_chk_guard. If they differ, a
 * buffer overflow has occurred (the canary was overwritten), and
 * the compiler calls __stack_chk_fail().
 *
 * WHY THREAD-LOCAL ?
 *
 * On a system with threads, each thread must have its own canary,
 * because a thread could be reading the canary while another
 * thread is writing it. Thread-local storage (TLS) guarantees
 * that each thread has its own copy.
 *
 * On a system without threads, a plain global works.
 *
 * We use the __HAJ_THREAD_LOCAL macro defined in bits/compiler.h.
 * If threads are not supported, the macro expands to nothing and
 * __stack_chk_guard is a plain global.
 *
 * INITIALIZATION
 *
 * The canary must be initialized with a random value at program
 * startup. If it is always 0 (or any fixed value), an attacker
 * can easily guess it and bypass the protection.
 *
 * Three sources are tried, in order of preference:
 *
 *   1. AT_RANDOM from the auxiliary vector. The kernel fills
 *      16 bytes of random at program startup, before _start
 *      runs. This is free, non-blocking, and always present on
 *      Linux since 2.6.29 (2009). getauxval(AT_RANDOM) returns
 *      its address.
 *
 *   2. getrandom(2). Cryptographically strong, but a syscall
 *      and may block during early boot if the entropy pool is
 *      not yet initialized.
 *
 *   3. A mix of ASLR-derived values (monotonic clock, pid,
 *      addresses of local variables and of this function). Not
 *      cryptographic, but unpredictable enough to defeat a
 *      remote attacker without an info leak.
 *
 * Whatever the source, the low byte is zeroed at the end so
 * that the canary contains a NUL byte. This stops string-based
 * overflows (strcpy, sprintf, ...) from copying the canary out
 * of memory: the first NUL terminates the copy.
 */

#include <bits/os.h>
#include <bits/compiler.h>
#include <bits/types.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/random.h>
#include <sys/auxv.h>

/* Forward declaration to avoid warnings */
void __haj_init_stack_guard(void);

/**
 * @brief The stack canary value.
 *
 * This variable is used by the compiler to detect stack
 * corruption. It is initialized at program startup with a
 * random value.
 *
 * On a system with threads, this variable is thread-local: each
 * thread has its own copy. On a single-threaded system, it is a
 * plain global.
 */
__HAJ_THREAD_LOCAL __haj_uintptr	__stack_chk_guard = 0;

/**
 * @brief Try to fill the canary from AT_RANDOM.
 *
 * @return 1 on success, 0 if AT_RANDOM is not available.
 */
static int	haj_guard_from_at_random(void)
{
	const void *rnd;

	/*
	 * On non-ELF platforms (Windows), getauxval is stubbed
	 * and returns 0 for every type. We treat that as "not
	 * available".
	 */
	rnd = (const void *)getauxval(AT_RANDOM);
	if (rnd == 0)
		return (0);

	/*
	 * Copy sizeof(__stack_chk_guard) bytes. AT_RANDOM is
	 * guaranteed to be at least 16 bytes on every platform
	 * that provides it.
	 */
	memcpy(&__stack_chk_guard, rnd, sizeof(__stack_chk_guard));
	return (1);
}

/**
 * @brief Try to fill the canary from getrandom(2).
 *
 * @return 1 on success, 0 if getrandom is not available or fails.
 */
static int	haj_guard_from_getrandom(void)
{
	ssize_t	n;

	n = getrandom(&__stack_chk_guard, sizeof(__stack_chk_guard), 0);
	return (n == (ssize_t)sizeof(__stack_chk_guard));
}

/**
 * @brief Fill the canary from a mix of ASLR-derived values.
 *
 * This is the last resort. It uses the monotonic clock, the
 * process id, and two addresses (one on the stack, one in the
 * text segment). Even if each source is partially predictable,
 * the XOR of all of them is not, without an info leak. The
 * result is then passed through the MurmurHash3 finalizer to
 * spread the bits.
 */
static void	haj_guard_from_aslr(void)
{
	__haj_uintptr	canary = 0;
	struct timespec	ts;
	__haj_uintptr	local;

	/* Monotonic clock: nanoseconds since boot. */
	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
		canary ^= (__haj_uintptr)ts.tv_sec;
		canary ^= ((__haj_uintptr)ts.tv_nsec) << 17;
	}

	/* pid: randomized on Linux. */
	canary ^= ((__haj_uintptr)getpid()) << 32;

	/* Address of a local variable: stack ASLR. */
	canary ^= (__haj_uintptr)&local;

	/* Address of this function: text ASLR. */
	canary ^= (__haj_uintptr)&haj_guard_from_aslr;

	/*
	 * MurmurHash3 finalizer. Spreads the bits so that
	 * partially correlated sources do not produce a
	 * partially predictable canary.
	 */
	canary ^= canary >> 33;
	canary *= 0xFF51AFD7ED558CCDULL;
	canary ^= canary >> 33;
	canary *= 0xC4CEB9FE1A85EC53ULL;
	canary ^= canary >> 33;

	__stack_chk_guard = canary;
}

/**
 * @brief Initialize the stack canary.
 *
 * This function initializes the stack canary with a random value.
 * It should be called at program startup.
 *
 * The function is idempotent: calling it twice does nothing the
 * second time. This matters because it may be called both from
 * _start and from a constructor, and the constructor may run
 * before or after _start depending on the platform.
 */
void	__haj_init_stack_guard(void)
{
	/* Already initialized. */
	if (__stack_chk_guard != 0)
		return;

	/*
	 * Preferred source: AT_RANDOM from the auxiliary vector.
	 * Free, non-blocking, always present on Linux since 2009.
	 */
	if (!haj_guard_from_at_random()) {
		/*
		 * Second try: getrandom(2). May block during early
		 * boot, hence the check.
		 */
		if (!haj_guard_from_getrandom()) {
			/*
			 * Last resort: ASLR-derived values.
			 */
			haj_guard_from_aslr();
		}
	}

	/*
	 * The low byte is set to 0 so that the canary contains a
	 * NUL byte, which stops string-based overflows from
	 * copying it.
	 */
	__stack_chk_guard &= ~(__haj_uintptr)0xFF;

	/*
	 * If everything failed and the canary is still 0, use a
	 * non-recognizable constant. This should never happen on
	 * a platform with AT_RANDOM or getrandom, but it keeps
	 * the canary non-zero on truly minimal systems.
	 */
	if (__stack_chk_guard == 0)
		__stack_chk_guard = 0x00000aff;
}
