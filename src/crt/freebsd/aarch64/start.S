/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/*
 * _start - FreeBSD aarch64 entry point.
 *
 * FreeBSD and Linux share the AAPCS64 ABI on aarch64, and the
 * initial stack layout is identical. This file is therefore
 * almost identical to the Linux aarch64 version; it is kept
 * separate only for future FreeBSD-specific changes.
 *
 * GLOBALS SET BY _start
 *
 *   __haj_argc, __progname, environ, __haj_auxv.
 *   See src/crt/linux/x86_64/start.S for the full description.
 */

.text
.global _start
.type   _start, %function

.extern __haj_argc
.extern __progname
.extern environ
.extern __haj_auxv

_start:
	/* argc */
	ldr	x0, [sp]
	adrp	x5, __haj_argc
	add	x5, x5, :lo12:__haj_argc
	str	w0, [x5]

	/* argv */
	add	x1, sp, #8

	/* __progname = argv[0] */
	ldr	x3, [x1]
	adrp	x4, __progname
	add	x4, x4, :lo12:__progname
	str	x3, [x4]

	/* envp = argv + (argc+1)*8 */
	add	x2, x1, x0, lsl #3
	add	x2, x2, #8
	adrp	x4, environ
	add	x4, x4, :lo12:environ
	str	x2, [x4]

	/* Walk envp to find the auxv */
	mov	x3, x2
.Lauxv_loop:
	ldr	x4, [x3]
	cbz	x4, .Lauxv_found
	add	x3, x3, #8
	b	.Lauxv_loop
.Lauxv_found:
	add	x3, x3, #8
	adrp	x4, __haj_auxv
	add	x4, x4, :lo12:__haj_auxv
	str	x3, [x4]

	/* Align stack */
	and	sp, sp, #-16

	/* main(argc, argv, envp) */
	bl	main

	/* exit(main_return_value) */
	bl	exit

	/* Safety net */
1:
	wfe
	b	1b

.size	_start, .-_start
