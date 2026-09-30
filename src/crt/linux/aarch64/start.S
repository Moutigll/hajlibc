/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/*
 * _start - Linux aarch64 entry point.
 *
 * This file is the aarch64 equivalent of the x86_64 version.
 *
 * DIFFERENCES FROM x86_64
 *
 *   1. Registers are named x0-x30 (64-bit). w0-w30 are the
 *      32-bit views.
 *   2. The syscall instruction is `svc #0` (Supervisor Call),
 *      not `syscall`.
 *   3. Function arguments are passed in x0-x7 (up to 8 args),
 *      not rdi, rsi, rdx, rcx, r8, r9.
 *   4. The return address is in x30 (link register), not on
 *      the stack. `bl` (Branch with Link) stores the return
 *      address in x30.
 *   5. The stack pointer is `sp`, not `rsp`. It must be
 *      16-byte aligned at all times.
 *   6. The kernel does NOT pass argc in a register. It is on
 *      the stack, at [sp].
 *
 * REGISTERS (aarch64)
 *
 *   x0-x7  : function arguments and return value
 *   x8     : syscall number (indirect result register)
 *   x9-x15 : temporary, caller-saved
 *   x16-x17: intra-procedure call scratch
 *   x18    : platform register (reserved on some systems)
 *   x19-x28: callee-saved
 *   x29    : frame pointer (fp)
 *   x30    : link register (lr, return address)
 *   sp     : stack pointer
 *   pc     : program counter (not directly accessible)
 *
 * The stack at entry as the same layout as x86_64, except that the pointers are 8 bytes each (64-bit).
 */

.text
.global _start
.type   _start, %function

.extern __haj_argc
.extern __progname
.extern environ
.extern __haj_auxv
.extern __haj_run_ctors

_start:
	/*
	 * Load argc from [sp].
	 *
	 * `ldr x0, [sp]` loads the 8 bytes at the address in sp into
	 * x0. x0 is the 1st argument register for function calls.
	 *
	 * After: x0 = argc
	 */
	ldr	x0, [sp]

	/* Save argc in __haj_argc (32-bit store: argc is `int`). */
	adrp	x5, __haj_argc
	add	x5, x5, :lo12:__haj_argc
	str	w0, [x5]

	/*
	 * Compute argv = sp + 8.
	 *
	 * `add x1, sp, #8` adds 8 to sp and stores the result in x1.
	 * x1 is the 2nd argument register.
	 *
	 * After: x1 = address of argv[0]
	 */
	add	x1, sp, #8

	/*
	 * Store argv[0] into __progname.
	 *
	 * __progname is a global variable defined in src/crt/progname.c.
	 * It holds the program name (argv[0]) so that diagnostic
	 * functions like __assert_fail can print it.
	 *
	 * x1 points to argv, so [x1] is argv[0]. We load it into x3,
	 * then load the address of __progname into x4 using adrp
	 * (address of page) + add :lo12: (low 12 bits), and store
	 * x3 into it.
	 *
	 * adrp + :lo12: is the standard aarch64 sequence to access
	 * a global variable's address when it may be farther than
	 * ±1 MiB from the current PC.
	 *
	 * After: __progname = argv[0]
	 */
	ldr	x3, [x1]			/* x3 = argv[0] */
	adrp	x4, __progname			/* x4 = page of __progname */
	add	x4, x4, :lo12:__progname	/* x4 = address of __progname */
	str	x3, [x4]			/* __progname = argv[0] */

	/*
	 * Compute envp = argv + (argc + 1) * 8.
	 *
	 * `add x2, x1, x0, lsl #3` adds x1 to (x0 << 3) and stores
	 * in x2. `lsl #3` is "logical shift left by 3", equivalent
	 * to multiplying by 8.
	 *
	 * Then `add x2, x2, #8` adds 8 to skip the NULL terminator.
	 *
	 * After: x2 = address of envp[0]
	 */
	add	x2, x1, x0, lsl #3
	add	x2, x2, #8

	/* Save envp in environ. */
	adrp	x4, environ
	add	x4, x4, :lo12:environ
	str	x2, [x4]

	/*
	 * Walk envp to find the auxiliary vector.
	 *
	 * The auxv starts right after the NULL terminator of envp.
	 * We scan forward until we hit a NULL pointer, then skip
	 * that NULL. The result is the address of auxv[0].
	 */
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

	/* Save argc, argv, envp in registers to avoid overwriting them. */
	mov	x5, x0	/* save argc */
	mov	x6, x1	/* save argv */
	mov	x7, x2	/* save envp */

	/*
	 * Call __haj_run_ctors() to run all constructors.
	 */
	bl	__haj_run_ctors

	/* Restore argc, argv, envp from registers. */
	mov	x0, x5	/* restore argc */
	mov	x1, x6	/* restore argv */
	mov	x2, x7	/* restore envp */

	/*
	 * Call main.
	 *
	 * `bl main` (Branch with Link) does two things:
	 *   1. Stores the address of the next instruction in x30.
	 *   2. Jumps to main.
	 *
	 * main receives:
	 *   x0 = argc
	 *   x1 = argv
	 *   x2 = envp
	 *
	 * main returns its value in x0.
	 */
	bl	main

	/*
	 * Call exit(main_return_value).
	 *
	 * x0 already contains main's return value, which is exactly
	 * what exit expects as its 1st argument. No move needed.
	 */
	bl	exit

	/*
	 * Safety net.
	 *
	 * `wfe` (Wait For Event) is a low-power hint. In user mode,
	 * it may be a no-op. The infinite loop prevents falling off
	 * the end of the function.
	 */
1:
	wfe
	b	1b

.size	_start, .-_start
