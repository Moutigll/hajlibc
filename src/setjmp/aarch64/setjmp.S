/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/*
 * setjmp - aarch64 (AAPCS64 ABI).
 *
 * This file works on Linux, FreeBSD, and Darwin. Darwin requires
 * an extra underscore prefix on C symbol names.
 *
 * REGISTERS SAVED (aarch64)
 *
 * Callee-saved integer registers : x19-x28
 * Frame pointer                  : x29
 * Link register                  : x30 (return address)
 * Stack pointer                  : sp
 * Callee-saved FP registers      : d8-d15 (64-bit)
 *
 * Caller-saved registers (x0-x18) are not saved.
 *
 * jmp_buf OFFSETS
 *
 *   offset   0 : x19
 *   offset   8 : x20
 *   offset  16 : x21
 *   offset  24 : x22
 *   offset  32 : x23
 *   offset  40 : x24
 *   offset  48 : x25
 *   offset  56 : x26
 *   offset  64 : x27
 *   offset  72 : x28
 *   offset  80 : x29 (fp)
 *   offset  88 : x30 (lr, return address)
 *   offset  96 : sp
 *   offset 104 : d8
 *   offset 112 : d9
 *   offset 120 : d10
 *   offset 128 : d11
 *   offset 136 : d12
 *   offset 144 : d13
 *   offset 152 : d14
 *   offset 160 : d15
 *
 * LINK REGISTER (x30)
 *
 * On aarch64, the return address of a function is stored in the
 * link register x30 by the `bl` instruction. We save x30 as the
 * return address. When longjmp restores x30 and executes `ret`,
 * the PC jumps to the saved address.
 */

# if defined(__APPLE__)
#  define GLUE(a, b)	a ## b
#  define XGLUE(a, b)	GLUE(a, b)
#  define CSYM(x)		XGLUE(_, x)
# else
#  define CSYM(x)		x
# endif

.text

/* ---------------------------------------------------------------------------
 * setjmp / _setjmp
 * --------------------------------------------------------------------------- */

.global CSYM(setjmp)
.global CSYM(_setjmp)
.type   CSYM(setjmp), %function
.type   CSYM(_setjmp), %function

CSYM(setjmp):
CSYM(_setjmp):
	/* Save x19-x28. */
	stp	x19, x20, [x0, #0]
	stp	x21, x22, [x0, #16]
	stp	x23, x24, [x0, #32]
	stp	x25, x26, [x0, #48]
	stp	x27, x28, [x0, #64]

	/* Save x29 (fp) and x30 (lr, return address). */
	stp	x29, x30, [x0, #80]

	/* Save the stack pointer. */
	mov	x2, sp
	str	x2, [x0, #96]

	/* Save FP registers d8-d15. */
	stp	d8,  d9,  [x0, #104]
	stp	d10, d11, [x0, #120]
	stp	d12, d13, [x0, #136]
	stp	d14, d15, [x0, #152]

	/* Return 0. */
	mov	w0, #0
	ret

.size	CSYM(setjmp), .-CSYM(setjmp)
.size	CSYM(_setjmp), .-CSYM(_setjmp)

/* ---------------------------------------------------------------------------
 * __haj_sigsetjmp
 * ---------------------------------------------------------------------------
 *
 * Second argument `savesigs` is in w1. If non-zero, call the
 * C helper to save the signal mask.
 */

.global CSYM(__haj_sigsetjmp)
.type   CSYM(__haj_sigsetjmp), %function

CSYM(__haj_sigsetjmp):
	/* Save x19-x28. */
	stp	x19, x20, [x0, #0]
	stp	x21, x22, [x0, #16]
	stp	x23, x24, [x0, #32]
	stp	x25, x26, [x0, #48]
	stp	x27, x28, [x0, #64]

	/* Save x29 (fp) and x30 (lr). */
	stp	x29, x30, [x0, #80]

	/* Save the stack pointer. */
	mov	x2, sp
	str	x2, [x0, #96]

	/* Save FP registers. */
	stp	d8,  d9,  [x0, #104]
	stp	d10, d11, [x0, #120]
	stp	d12, d13, [x0, #136]
	stp	d14, d15, [x0, #152]

	/* If savesigs == 0, skip the mask save. */
	cbz	w1, 1f

	/*
	 * aarch64 requires sp to be 16-byte aligned at all times.
	 * The ABI also requires sp to be 16-byte aligned before a
	 * `bl` instruction. Since the entry sp is already aligned,
	 * we do not need to adjust it.
	 */
	bl	CSYM(__haj_sigsetjmp_save)

1:
	/* Return 0. */
	mov	w0, #0
	ret

.size	CSYM(__haj_sigsetjmp), .-CSYM(__haj_sigsetjmp)
