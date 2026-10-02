/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/*
 * start - Darwin arm64 entry point.
 *
 * DIFFERENCES FROM LINUX aarch64
 *
 *   1. The symbol is `start`, not `_start`.
 *
 *   2. The kernel passes argc/argv/envp in registers:
 *        x0 = argc
 *        x1 = argv
 *        x2 = envp
 *
 *   3. No auxiliary vector on the stack.
 *
 * GLOBALS SET BY start
 *
 *   __haj_argc, __progname, environ.
 *   See src/crt/darwin/x86_64/start.S for the reasoning about
 *   __haj_auxv and the symbol mangling.
 */

.text
.global start
.type   start, %function

.extern __haj_argc
.extern ___progname
.extern _environ
.extern __haj_run_ctors

start:
	/* __haj_argc = argc */
	adrp	x4, __haj_argc
	add	x4, x4, :lo12:__haj_argc
	str	w0, [x4]

	/* __progname = argv[0] */
	ldr	x3, [x1]
	adrp	x4, ___progname
	add	x4, x4, :lo12:___progname
	str	x3, [x4]

	/* environ = envp */
	adrp	x4, _environ
	add	x4, x4, :lo12:_environ
	str	x2, [x4]

	/* Align stack */
	and	sp, sp, #-16

	/* Save argc, argv and envp in registers to avoid overwriting them. */
	mov	x19, x0	/* save argc */
	mov	x20, x1	/* save argv */
	mov	x21, x2	/* save envp */

	/* Call __haj_run_ctors() to run all constructors. */
	bl	__haj_run_ctors

	/* Restore argc, argv and envp from registers. */
	mov	x0, x19	/* restore argc */
	mov	x1, x20	/* restore argv */
	mov	x2, x21	/* restore envp */

	/* main(argc, argv, envp) */
	bl	main

	/* exit(main_return_value) */
	bl	exit

	/* Safety net */
1:
	wfe
	b	1b

.size	start, .-start
