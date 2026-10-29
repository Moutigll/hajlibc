/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/*
 * ============================================================================
 * hajlibc.ld - Linker script for programs linked against hajlibc.
 * ============================================================================
 *
 * WHAT A LINKER SCRIPT IS
 * -----------------------
 * The linker (`ld`, invoked by `cc` at the last stage) assembles object
 * files into one executable. The built-in default script expects glibc's
 * crt1.o / crti.o / crtn.o, `__libc_start_main`, glibc's TCB layout...
 * None of that exists here (`-nostartfiles -nodefaultlibs -static`), so
 * this file replaces it: it says where each section goes, which PT_LOAD
 * segments to create, and which SYMBOLS the runtime (`src/crt/`,
 * `src/thread/internal/tcb.c`) uses to find its data.
 *
 * It is NOT a runtime library: it only organises bytes in the output file.
 *
 * MEMORY MODEL
 * ------------
 *
 *   +----------------------+	0x400000 (R + X)
 *   | .text .init .fini	|	code
 *   +----------------------+	ALIGN(4096) (R)
 *   | .rodata				|	constants, strings
 *   +----------------------+	ALIGN(4096) (R + W)
 *   | .preinit_array		|
 *   | .init_array			|	constructors
 *   | .fini_array			|	destructors
 *   | .tdata				|	TLS initialisation image (template)
 *   | .tbss				|	TLS zero-init description (NO address space)
 *   | .data .got .dynamic	|
 *   | .bss					|
 *   +----------------------+
 *
 * Base 0x400000 is the conventional address of a non-PIE x86_64
 * executable. We compile with `-fno-pie -fno-pic`, so no relocation is
 * processed at runtime.
 *
 * THREAD-LOCAL STORAGE (TLS)
 * --------------------------
 * x86_64 uses the "variant II" TLS layout (glibc calls it
 * TLS_TCB_AT_TP): the thread pointer TP (%fs) points AT the TCB, and the
 * thread's TLS variables sit just BELOW it:
 *
 *	 high address
 *	 +------------------+
 *	 |	TCB (struct)	|	<-- TP == %fs, self pointer at %fs:0
 *	 +------------------+
 *	 |	.tbss copy		|	zero-initialised TLS variables
 *	 +------------------+
 *	 |	.tdata copy		|	pre-initialised TLS variables
 *	 +------------------+	<-- TP - tls_memsz
 *	 low address
 *
 * The compiler/linker emit `%fs:offset` with
 *
 *	 offset = var_address - (tls_start + tls_memsz)	  (negative)
 *
 * where `tls_start` is the vma of the first TLS output section and
 * `tls_memsz` its distance to the END of the last one (.tbss). So:
 *
 *	 TP == __tbss_end  (as an address *relative to the template*)
 *
 * (aarch64 is variant I instead: TLS variables are ABOVE TP, with 16
 * bytes reserved at TP. See tcb.c.)
 *
 * WHY THE MAIN THREAD'S TLS BLOCK AND TCB ARE NOT RESERVED HERE
 * -------------------------------------------------------------
 * `.tbss` is a NOBITS | TLS section. `ld` does NOT advance the location
 * counter past it: the sections that follow (.data, .bss...) are placed
 * at the same addresses, overlaying it. `.tdata` / `.tbss` are therefore
 * only a TEMPLATE describing what each thread's private block must look
 * like; the template addresses are never live memory.
 *
 * Consequences:
 *
 *   - `. = . + 4096;` inside .tbss does not reserve any memory. It only
 *	 inflates the section size (and thus moves TP by 4096 for the
 *	 linker), while .data lands inside the "reserve". Using it for the
 *	 TCB corrupts .data. (It is also why a separate NOLOAD section
 *	 placed after .tbss was re-emitted at `__tbss_start`: `.` had not
 *	 moved.)
 *   - Nothing may be placed after `__tbss_end` INSIDE .tbss: that would
 *	 move TP away from the TCB.
 *   - Every thread, main included, needs a real memory area holding
 *	 [TLS block][TCB]. For the main thread it is a static buffer in
 *	 tcb.c (`g_mainArea`, in .bss). For other threads it is allocated by
 *	 pthread_create.
 */

OUTPUT_FORMAT(elf64-x86-64)	/* aarch64: elf64-littleaarch64 */
OUTPUT_ARCH(i386:x86-64)	/* aarch64: aarch64 */
ENTRY(_start)

PHDRS
{
		text	PT_LOAD FLAGS(5);	/* R + X : code */
		rodata	PT_LOAD FLAGS(4);	/* R	 : constants */
		data	PT_LOAD FLAGS(6);	/* R + W : initialized data, TLS template, BSS */
}

SECTIONS
{
		/*
		 * Fixed base address. `-fno-pie -fno-pic` makes every reference
		 * to a global a direct load from this address.
		 */
		. = 0x400000;

		/*
		 * .text : executable code (+ legacy .init / .fini).
		 */
		.text : ALIGN(4096)
		{
				__text_start = .;
				*(.text .text.*)
				*(.init)
				*(.fini)
				__text_end = .;
		} :text

		/*
		 * .rodata : read-only data. Own PT_LOAD, R only, so any write
		 * faults immediately.
		 */
		.rodata : ALIGN(4096)
		{
				*(.rodata .rodata.*)
		} :rodata

		/*
		 * The R/W segment must start on its own page: otherwise the last
		 * page of .rodata would be shared with the data segment and end
		 * up writable.
		 */
		. = ALIGN(4096);

		/*
		 * .preinit_array : very early constructors (nothing emitted by
		 * hajlibc itself, kept for external code).
		 */
		.preinit_array : ALIGN(8)
		{
				PROVIDE_HIDDEN(__preinit_array_start = .);
				KEEP(*(.preinit_array))
				PROVIDE_HIDDEN(__preinit_array_end = .);
		} :data

		/*
		 * .init_array : constructors. `__haj_run_ctors()` iterates
		 * [__init_array_start, __init_array_end] before main().
		 * SORT_BY_INIT_PRIORITY honours `constructor(N)`: lower N first.
		 */
		.init_array : ALIGN(8)
		{
				PROVIDE_HIDDEN(__init_array_start = .);
				KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
				KEEP(*(.init_array))
				PROVIDE_HIDDEN(__init_array_end = .);
		} :data

		/*
		 * .fini_array : destructors, called in reverse order before
		 * _exit().
		 */
		.fini_array : ALIGN(8)
		{
				PROVIDE_HIDDEN(__fini_array_start = .);
				KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
				KEEP(*(.fini_array))
				PROVIDE_HIDDEN(__fini_array_end = .);
		} :data

		/*
		 * .tdata : initialisation image of every `__thread` variable
		 * with a non-zero initializer.
		 *
		 * This is a TEMPLATE: tcb.c / pthread_create copy it into each
		 * thread's private block. Nobody may write to it at runtime.
		 *
		 * ALIGN(64): the block alignment tcb.c assumes (HAJ_TLS_ALIGN).
		 * It also becomes the alignment of the TLS segment.
		 */
		.tdata : ALIGN(64)
		{
				__tdata_start = .;
				*(.tdata .tdata.* .gnu.linkonce.td.*)
				__tdata_end = .;
		} :data

		/*
		 * .tbss : zero-initialised TLS variables (NOBITS, TLS).
		 *
		 * Describes the part of each thread's block that must be zeroed.
		 * Occupies NO address space (see header): the following sections
		 * overlay it, so these addresses must never be read or written
		 * as memory.
		 *
		 * Nothing else goes in here. In particular NO reserve after
		 * `__tbss_end`: the linker defines TP as the end of this section.
		 *
		 * `. = ALIGN(64)` pads the section so that
		 * (__tbss_end - __tdata_start) is a multiple of 64.
		 */
		.tbss : ALIGN(64)
		{
				__tbss_start = .;
				*(.tbss .tbss.* .gnu.linkonce.tb.*)
				*(.tcommon)
				. = ALIGN(64);
				__tbss_end = .;
		} :data

		/*
		 * TLS metadata, read by tcb.c and pthread_create.
		 *
		 * __tls_size : bytes of the TLS block of ONE thread (variables
		 *			  only, no TCB), i.e. what sits below TP on x86_64.
		 *			  ABSOLUTE() because the two symbols live in
		 *			  different output sections.
		 * __tls_align: alignment the block must satisfy.
		 */
		PROVIDE_HIDDEN(__tls_size  = ABSOLUTE(__tbss_end) - ABSOLUTE(__tdata_start));
		PROVIDE_HIDDEN(__tls_align = MAX(ALIGNOF(.tdata), ALIGNOF(.tbss)));

		/*
		 * .data : initialized, writable data (+ GOT / .dynamic, empty
		 * here because we are static, kept for tools that expect them).
		 *
		 * Starts on a fresh page. It overlays the .tbss template range;
		 * that is fine, see above.
		 */
		.data : ALIGN(4096)
		{
				__data_start = .;
				*(.data .data.*)
				*(.got .got.plt)
				*(.dynamic)
				_edata = .;
		} :data

		/*
		 * .bss : zero-initialised data. No file space; the kernel
		 * zero-fills it. The main thread's TLS area (`g_mainArea` in
		 * tcb.c) lives here.
		 */
		.bss : ALIGN(4096)
		{
				__bss_start = .;
				*(.bss .bss.*)
				*(COMMON)
				__bss_end = .;
		} :data

		/*
		 * Keep the linker script and tcb.c in sync: the main thread's
		 * TLS block must fit in HAJ_MAIN_TLS_MAX (tcb.c). Bump both
		 * together.
		 */
		ASSERT(ABSOLUTE(__tbss_end) - ABSOLUTE(__tdata_start) <= 16384,
						"main-thread TLS block exceeds HAJ_MAIN_TLS_MAX (see tcb.c)")

		/*
		 * Discarded sections: compiler version strings, stack-executable
		 * markers, and unwind tables (we build with
		 * -fno-asynchronous-unwind-tables).
		 */
		/DISCARD/ :
		{
				*(.comment)
				*(.note.GNU-stack)
				*(.eh_frame)
				*(.eh_frame_hdr)
		}
}
