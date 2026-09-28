/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/*
 * longjmp - aarch64.
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
 * longjmp / _longjmp
 * ---------------------------------------------------------------------------
 *
 * Signature: void longjmp(jmp_buf env, int val);
 *   x0 = env
 *   w1 = val
 */

.global CSYM(longjmp)
.global CSYM(_longjmp)
.type   CSYM(longjmp), %function
.type   CSYM(_longjmp), %function

CSYM(longjmp):
CSYM(_longjmp):
	/* If val == 0, use 1 instead. */
	cmp	w1, #0
	cinc	w1, w1, eq	/* w1 = (w1 == 0) ? w1 + 1 : w1 */

	/* Restore x19-x28. */
	ldp	x19, x20, [x0, #0]
	ldp	x21, x22, [x0, #16]
	ldp	x23, x24, [x0, #32]
	ldp	x25, x26, [x0, #48]
	ldp	x27, x28, [x0, #64]

	/* Restore x29 (fp) and x30 (lr). */
	ldp	x29, x30, [x0, #80]

	/* Restore FP registers. */
	ldp	d8,  d9,  [x0, #104]
	ldp	d10, d11, [x0, #120]
	ldp	d12, d13, [x0, #136]
	ldp	d14, d15, [x0, #152]

	/* Restore the stack pointer. */
	ldr	x2, [x0, #96]
	mov	sp, x2

	/* Return val from setjmp. */
	mov	w0, w1

	/* Jump to the saved return address in x30. */
	ret

.size	CSYM(longjmp), .-CSYM(longjmp)
.size	CSYM(_longjmp), .-CSYM(_longjmp)
