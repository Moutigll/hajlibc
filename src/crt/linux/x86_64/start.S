/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/*
 * _start - Linux x86_64 entry point.
 *
 * WHAT THIS FILE DOES ?
 *
 * This is the very first code executed after the kernel loads the
 * binary. The kernel does NOT call main(). It jumps to the address
 * stored in the ELF header's e_entry field, which the linker sets
 * to _start.
 *
 * Our job in _start:
 *   1. Read argc, argv, envp, and auxv from the initial stack.
 *   2. Store argv[0] into __progname (for diagnostics).
 *   3. Store argc in __haj_argc.
 *   4. Store envp in environ.
 *   5. Store the auxv pointer in __haj_auxv.
 *   6. Call main(argc, argv, envp).
 *   7. Call exit(main_return_value).
 *
 * REGISTERS (x86_64 System V ABI)
 *
 *   rax  (64-bit) : accumulator, also used for syscall numbers
 *   rbx           : callee-saved, general purpose
 *   rcx           : counter, also clobbered by syscall
 *   rdx           : data, 3rd function argument
 *   rsi           : source index, 2nd function argument
 *   rdi           : destination index, 1st function argument
 *   rbp           : base pointer (frame pointer)
 *   rsp           : stack pointer (points to the top of the stack)
 *   r8-r15        : additional general-purpose registers
 *
 * STACK AT ENTRY
 *
 * When _start begins, the stack looks like this (addresses grow
 * downward, so rsp points to the LOWEST address):
 *
 *   rsp ->  +------------------+
 *           | argc             |  8 bytes (number of arguments)
 *           +------------------+
 *           | argv[0]          |  8 bytes (pointer to program name)
 *           +------------------+
 *           | argv[1]          |  8 bytes (pointer to 1st argument)
 *           +------------------+
 *           | ...              |
 *           +------------------+
 *           | NULL             |  8 bytes (marks end of argv)
 *           +------------------+
 *           | envp[0]          |  8 bytes (pointer to "KEY=VALUE")
 *           +------------------+
 *           | envp[1]          |  8 bytes
 *           +------------------+
 *           | ...              |
 *           +------------------+
 *           | NULL             |  8 bytes (marks end of envp)
 *           +------------------+
 *           | auxv             |  auxiliary vector (kernel info)
 *           +------------------+
 *
 * The Linux kernel sets up this layout before jumping to _start.
 * We read argc from [rsp], argv from rsp+8, and envp right after
 * the NULL terminator of argv.
 *
 * CALLING CONVENTION (System V AMD64 ABI)
 *
 * Integer arguments are passed in: rdi, rsi, rdx, rcx, r8, r9.
 * Additional arguments go on the stack.
 *
 * The stack must be 16-byte aligned BEFORE the `call` instruction.
 * After `call` pushes the 8-byte return address, rsp % 16 == 8
 * at the callee's entry. This is the correct state for the ABI.
 *
 * The return value of a function is in rax (for integers and
 * pointers).
 *
 */

.text
.global _start
.type   _start, @function

.extern __haj_argc
.extern __progname
.extern environ
.extern __haj_auxv

_start:
	/*
	 * Step 1: read argc from the stack.
	 *
	 * `movq (%rsp), %rdi` copies 8 bytes from the address in rsp
	 * to rdi. The parentheses mean "dereference" - we read the
	 * VALUE stored at that address, not the address itself.
	 *
	 * Before: rsp = 0x7fff...
	 *         [rsp] = 3 (the number of arguments, for example)
	 * After:  rdi = 3
	 *
	 * rdi is the 1st argument register in the System V ABI. We
	 * are preparing to call main(argc, ...).
	 */
	movq	(%rsp), %rdi

	/* Save argc in __haj_argc (32-bit store: argc is `int`). */
	movl	%edi, __haj_argc(%rip)

	/*
	 * Step 2: compute argv.
	 *
	 * `leaq 8(%rsp), %rsi` loads the EFFECTIVE ADDRESS (not the
	 * value) of rsp+8 into rsi. "lea" stands for Load Effective
	 * Address.
	 *
	 * Before: rsp = 0x7fff...
	 * After:  rsi = 0x7fff... + 8  (the address of argv[0])
	 *
	 * rsi is the 2nd argument register. We are preparing to call
	 * main(argc, argv, ...).
	 */
	leaq	8(%rsp), %rsi

	/*
	 * Step 3: store argv[0] into __progname.
	 *
	 * __progname is a global variable defined in src/crt/progname.c.
	 * It holds the program name (argv[0]) so that diagnostic
	 * functions like __assert_fail can print it.
	 *
	 * rsi points to argv, so (%rsi) is argv[0]. We load it into
	 * rax and store it into __progname using RIP-relative
	 * addressing.
	 *
	 * RIP-relative addressing is required for position-independent
	 * code and works in static binaries too. The linker resolves
	 * the offset at link time.
	 *
	 * After: __progname = argv[0]
	 */
	movq	(%rsi), %rax		/* rax = argv[0] */
	movq	%rax, __progname(%rip)	/* __progname = argv[0] */

	/*
	 * Step 4: compute envp.
	 *
	 * envp is located right after argv. argv has (argc + 1)
	 * entries: argc pointers, then a NULL terminator.
	 *
	 * The formula for envp is:
	 *   envp = argv + (argc + 1) * 8
	 *
	 * We compute it in one instruction:
	 *   leaq 8(%rsi,%rdi,8), %rdx
	 *
	 * The addressing mode `8(%rsi,%rdi,8)` means:
	 *   rsi + rdi*8 + 8
	 *
	 * Breaking it down:
	 *   - rsi     = argv (base)
	 *   - rdi     = argc
	 *   - rdi*8   = argc * 8 (each pointer is 8 bytes)
	 *   - +8      = skip the NULL terminator (argc entries + 1)
	 *
	 * So: rdx = argv + argc*8 + 8 = argv + (argc+1)*8
	 *
	 * rdx is the 3rd argument register. We are preparing to call
	 * main(argc, argv, envp).
	 */
	leaq	8(%rsi,%rdi,8), %rdx

	/* Save envp in environ. */
	movq	%rdx, environ(%rip)

	/*
	 * Step 4b: walk envp to find the auxiliary vector.
	 *
	 * The auxv starts right after the NULL terminator of envp.
	 * We scan forward until we hit a NULL pointer, then skip
	 * that NULL. The result is the address of auxv[0].
	 */
	movq	%rdx, %rax
.Lauxv_loop:
	cmpq	$0, (%rax)
	je	.Lauxv_found
	addq	$8, %rax
	jmp	.Lauxv_loop
.Lauxv_found:
	addq	$8, %rax
	movq	%rax, __haj_auxv(%rip)

	/*
	 * Step 5: align the stack.
	 *
	 * The System V ABI requires rsp to be 16-byte aligned at the
	 * `call` instruction. The kernel guarantees that rsp is
	 * 16-byte aligned at _start entry, so this instruction is
	 * actually redundant here. But it does not hurt, and it
	 * documents the requirement.
	 *
	 * `andq $-16, %rsp` masks the low 4 bits of rsp to 0. The
	 * immediate value -16 is 0xFFFFFFFFFFFFFFF0 in 64-bit
	 * two's complement. AND-ing with it clears bits 0-3.
	 *
	 * Before: rsp = 0x7fff...d (not aligned)
	 * After:  rsp = 0x7fff...0 (aligned to 16)
	 */
	andq	$-16, %rsp

	/*
	 * Step 6: call main.
	 *
	 * `call main` does two things:
	 *   1. Push the address of the next instruction (the return
	 *      address) onto the stack. rsp decreases by 8.
	 *   2. Jump to the address of main.
	 *
	 * main receives:
	 *   rdi = argc
	 *   rsi = argv
	 *   rdx = envp
	 *
	 * main returns its value in rax.
	 */
	call	main

	/*
	 * Step 7: call exit(main_return_value).
	 *
	 * main returned its int result in rax. We move it to rdi
	 * (the 1st argument register) to pass it to exit().
	 *
	 * `movq %rax, %rdi` copies the 8 bytes in rax to rdi.
	 */
	movq	%rax, %rdi

	/*
	 * `call exit` pushes the return address and jumps to exit.
	 * exit() is our own function (in src/crt/exit.c). It calls
	 * _exit() which terminates the process via the exit_group
	 * syscall.
	 *
	 * exit() should NEVER return. If it does, the code below
	 * runs.
	 */
	call	exit

	/*
	 * Step 8: safety net.
	 *
	 * This code should never be reached. exit() terminates the
	 * process. But if something goes wrong (e.g. exit returns
	 * because a syscall failed), we halt the CPU to avoid
	 * executing garbage.
	 *
	 * `hlt` halts the CPU until the next interrupt. In user
	 * mode, hlt is a privileged instruction and raises SIGSEGV.
	 * So this actually crashes cleanly instead of running wild.
	 *
	 * `jmp 1b` jumps back to label 1 (defined just above). The
	 * `b` suffix means "backward". This creates an infinite loop.
	 */
1:
	hlt
	jmp	1b

.size	_start, .-_start
