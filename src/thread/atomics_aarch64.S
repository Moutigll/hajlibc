/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/*
 * Out-of-line atomics helpers for aarch64.
 *
 * Clang with -moutline-atomics emits calls to helpers named
 *     __aarch64_<op><size>_<order>
 * for every atomic operation. These helpers are normally
 * provided by libgcc; since hajlibc is freestanding and does
 * not link against libgcc, we provide them here.
 *
 * WHY TWO IMPLEMENTATIONS
 *
 * aarch64 has two ways to do atomics:
 *
 *   1. LL/SC (Load-Linked / Store-Conditional), available
 *      since ARMv8.0. Uses a loop of LDXR/STXR pairs. Works
 *      everywhere but is slow under contention: if another
 *      core writes the same cache line, the store-conditional
 *      fails and the loop restarts.
 *
 *   2. LSE (Large System Extensions), available since
 *      ARMv8.1 (2014). Single-instruction atomics: LDADD,
 *      CAS, SWP. Fast even under heavy contention.
 *
 * -moutline-atomics makes GCC emit a call to a helper that
 * chooses at runtime which path to take based on HWCAP. The
 * helper checks a global flag once, then branches.
 *
 * RUNTIME CHOICE
 *
 * __haj_cpuHasLse is a global int, set once at startup by
 * __haj_cpuDetect_aarch64() (see features_aarch64.c) from the
 * HWCAP_ATOMICS bit of AT_HWCAP.
 *
 *   0  ->  use the LL/SC path
 *   1  ->  use the LSE path
 *
 * The flag is read on every call. It is a single L1-cached
 * load and a well-predicted branch, which costs less than the
 * atomic operation itself. libgcc uses the same approach.
 *
 * NAMING
 *
 *   __aarch64_<op><size>_<order>
 *
 *   <op>    : cas    compare and swap
 *             swp    exchange
 *             ldadd  atomic add
 *   <size>  : 4      32-bit (int)
 *             8      64-bit (long or pointer)
 *   <order> : acq_rel
 *             acq
 *             rel
 *
 * We only provide the _acq_rel variants. The atomics used in
 * hajlibc are declared with __ATOMIC_SEQ_CST, which GCC lowers
 * to _acq_rel calls on aarch64. If we ever use weaker orderings
 * (__ATOMIC_RELAXED, __ATOMIC_ACQUIRE, ...), we will need to
 * add the corresponding helpers.
 *
 * REGISTER CONVENTIONS
 *
 * The helpers follow the AAPCS64 calling convention. GCC
 * assumes the standard registers:
 *
 *   casN  : x0 = expected, x1 = new, x2 = address
 *           return: x0 = value read
 *   swpN  : x0 = new, x1 = address
 *           return: x0 = previous value
 *   ldaddN: x0 = value to add, x1 = address
 *           return: x0 = previous value
 */

.text

/* ----- 32-bit (int) helpers ----- */

/* -----
 * __aarch64_cas4_acq_rel
 *
 * Compare-and-swap on a 32-bit value.
 *
 *   w0 = expected value
 *   w1 = new value
 *   x2 = address of the target
 * Return:
 *   w0 = value read from memory
 *
 * If the value read equals w0, the new value is stored and
 * the operation succeeds. Otherwise, nothing is written and
 * w0 receives the current value so the caller can retry.
 * ----- */

.global __aarch64_cas4_acq_rel
.hidden __aarch64_cas4_acq_rel
.type   __aarch64_cas4_acq_rel, %function
__aarch64_cas4_acq_rel:
	adrp	x4, __haj_cpuHasLse
	add	x4, x4, :lo12:__haj_cpuHasLse
	ldr	w4, [x4]
	cbz	w4, .Lcas4_llsc

	/* LSE: cas w0, w1, [x2] — compares [x2] to w0, if equal
	 * stores w1, always puts the previous value in w0. */
	cas	w0, w1, [x2]
	ret

.Lcas4_llsc:
1:	ldaxr	w3, [x2]
	cmp	w3, w0
	b.ne	2f
	stlxr	w4, w1, [x2]
	cbnz	w4, 1b
2:	mov	w0, w3
	ret
.size	__aarch64_cas4_acq_rel, .-__aarch64_cas4_acq_rel

/* -----
 * __aarch64_swp4_acq_rel
 *
 * Atomic exchange on a 32-bit value.
 *
 *   w0 = new value
 *   x1 = address of the target
 * Return:
 *   w0 = previous value
 * ----- */

.global __aarch64_swp4_acq_rel
.hidden __aarch64_swp4_acq_rel
.type   __aarch64_swp4_acq_rel, %function
__aarch64_swp4_acq_rel:
	adrp	x2, __haj_cpuHasLse
	add	x2, x2, :lo12:__haj_cpuHasLse
	ldr	w2, [x2]
	cbz	w2, .Lswp4_llsc

	/* LSE: swpal ws, wt, [xn] — atomically stores wt into
	 * [xn] and returns the previous value in ws. Both
	 * operands are w0 because we want the new value on both
	 * sides: w0 is the value written and also the register
	 * that receives the old value. */
	swpal	w0, w0, [x1]
	ret

.Lswp4_llsc:
	mov	w2, w0
1:	ldaxr	w0, [x1]
	stlxr	w3, w2, [x1]
	cbnz	w3, 1b
	ret
.size	__aarch64_swp4_acq_rel, .-__aarch64_swp4_acq_rel

/* -----
 * __aarch64_ldadd4_acq_rel
 *
 * Atomic add on a 32-bit value.
 *
 *   w0 = value to add
 *   x1 = address of the target
 * Return:
 *   w0 = previous value
 *
 * GCC also uses this for subtract, by passing a negated
 * value.
 * ----- */

.global __aarch64_ldadd4_acq_rel
.hidden __aarch64_ldadd4_acq_rel
.type   __aarch64_ldadd4_acq_rel, %function
__aarch64_ldadd4_acq_rel:
	adrp	x2, __haj_cpuHasLse
	add	x2, x2, :lo12:__haj_cpuHasLse
	ldr	w2, [x2]
	cbz	w2, .Lldadd4_llsc

	/* LSE: ldaddal ws, wt, [xn] — computes [xn] = [xn] + wt
	 * and returns the old value in ws. w2 receives the old
	 * value, then we move it to w0 for the return. */
	ldaddal	w0, w2, [x1]
	mov	w0, w2
	ret

.Lldadd4_llsc:
1:	ldaxr	w2, [x1]
	add	w3, w2, w0
	stlxr	w4, w3, [x1]
	cbnz	w4, 1b
	mov	w0, w2
	ret
.size	__aarch64_ldadd4_acq_rel, .-__aarch64_ldadd4_acq_rel

/* ----- 64-bit (long / pointer) helpers ----- */

/* -----
 * __aarch64_cas8_acq_rel
 *
 * Same as cas4 but for a 64-bit value.
 *
 *   x0 = expected value
 *   x1 = new value
 *   x2 = address of the target
 * Return:
 *   x0 = value read from memory
 * ----- */

.global __aarch64_cas8_acq_rel
.hidden __aarch64_cas8_acq_rel
.type   __aarch64_cas8_acq_rel, %function
__aarch64_cas8_acq_rel:
	adrp	x4, __haj_cpuHasLse
	add	x4, x4, :lo12:__haj_cpuHasLse
	ldr	w4, [x4]
	cbz	w4, .Lcas8_llsc

	cas	x0, x1, [x2]
	ret

.Lcas8_llsc:
1:	ldaxr	x3, [x2]
	cmp	x3, x0
	b.ne	2f
	stlxr	w4, x1, [x2]
	cbnz	w4, 1b
2:	mov	x0, x3
	ret
.size	__aarch64_cas8_acq_rel, .-__aarch64_cas8_acq_rel

/* -----
 * __aarch64_swp8_acq_rel
 *
 *   x0 = new value
 *   x1 = address of the target
 * Return:
 *   x0 = previous value
 * ----- */

.global __aarch64_swp8_acq_rel
.hidden __aarch64_swp8_acq_rel
.type   __aarch64_swp8_acq_rel, %function
__aarch64_swp8_acq_rel:
	adrp	x2, __haj_cpuHasLse
	add	x2, x2, :lo12:__haj_cpuHasLse
	ldr	w2, [x2]
	cbz	w2, .Lswp8_llsc

	swpal	x0, x0, [x1]
	ret

.Lswp8_llsc:
	mov	x2, x0
1:	ldaxr	x0, [x1]
	stlxr	w3, x2, [x1]
	cbnz	w3, 1b
	ret
.size	__aarch64_swp8_acq_rel, .-__aarch64_swp8_acq_rel

/* -----
 * __aarch64_ldadd8_acq_rel
 *
 *   x0 = value to add
 *   x1 = address of the target
 * Return:
 *   x0 = previous value
 * ----- */

.global __aarch64_ldadd8_acq_rel
.hidden __aarch64_ldadd8_acq_rel
.type   __aarch64_ldadd8_acq_rel, %function
__aarch64_ldadd8_acq_rel:
	adrp	x2, __haj_cpuHasLse
	add	x2, x2, :lo12:__haj_cpuHasLse
	ldr	w2, [x2]
	cbz	w2, .Lldadd8_llsc

	ldaddal	x0, x2, [x1]
	mov	x0, x2
	ret

.Lldadd8_llsc:
1:	ldaxr	x2, [x1]
	add	x3, x2, x0
	stlxr	w4, x3, [x1]
	cbnz	w4, 1b
	mov	x0, x2
	ret
.size	__aarch64_ldadd8_acq_rel, .-__aarch64_ldadd8_acq_rel

.section .note.GNU-stack,"",@progbits
