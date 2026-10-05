/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 *
 * Minimal linker script for hajlibc.
 *
 * It defines the standard sections and, most importantly, the
 * boundaries of the .init_array and .fini_array sections so that
 * the C runtime can run constructors before main() and
 * destructors before _exit(). Without this script, the linker
 * does not create these symbols, and __attribute__((constructor))
 * silently does nothing.
 *
 * This script is used by programs that link against libhajc.a
 * with -nostartfiles. It replaces the default linker script
 * (usually /usr/lib/.../ldscripts/elf_x86_64.x).
 */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

PHDRS
{
	text    PT_LOAD    FLAGS(5);   /* RX */
	rodata  PT_LOAD    FLAGS(4);   /* R  */
	data    PT_LOAD    FLAGS(6);   /* RW */
}

SECTIONS
{
	. = 0x400000;

	.text : ALIGN(4096)
	{
		*(.text .text.*)
		*(.init)
		*(.fini)
	} :text

	.rodata : ALIGN(4096)
	{
		*(.rodata .rodata.*)
	} :rodata

	/*
	 * Pre-init array: called before .init_array, used by
	 * some runtimes for very early initialization. We keep it
	 * for compatibility.
	 */
	.preinit_array : ALIGN(8)
	{
		PROVIDE_HIDDEN(__preinit_array_start = .);
		KEEP(*(.preinit_array))
		PROVIDE_HIDDEN(__preinit_array_end = .);
	} :data

	/*
	 * Init array: called by __haj_run_ctors() before main().
	 * Each entry is a void(*)(void).
	 */
	.init_array : ALIGN(8)
	{
		PROVIDE_HIDDEN(__init_array_start = .);
		KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
		KEEP(*(.init_array))
		PROVIDE_HIDDEN(__init_array_end = .);
	} :data

	/*
	 * Fini array: called by __haj_run_dtors() before _exit().
	 * Order is reversed: the last constructor's destructor
	 * runs first.
	 */
	.fini_array : ALIGN(8)
	{
		PROVIDE_HIDDEN(__fini_array_start = .);
		KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
		KEEP(*(.fini_array))
		PROVIDE_HIDDEN(__fini_array_end = .);
	} :data

	.data : ALIGN(4096)
	{
		*(.data .data.*)
		*(.got .got.plt)
		*(.dynamic)
	} :data

	.bss : ALIGN(4096)
	{
		__bss_start = .;
		*(.bss .bss.*)
		*(COMMON)
		__bss_end = .;
	} :data

	/*
	 * Discard unneeded sections.
	 */
	/DISCARD/ :
	{
		*(.comment)
		*(.note.GNU-stack)
		*(.eh_frame)
		*(.eh_frame_hdr)
	}
}
