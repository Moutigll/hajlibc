/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file symbols.h
 * @brief Linker symbols for .text, .data, .bss, and TLS.
 * @Created: 2026/10/26 03:50:42 by Moutig
 * @Updated: 2026/10/26 07:53:56 by Moutig
 *
 * These are NOT variables: their addresses are filled in by the
 * linker script (mk/hajlibc.ld). Taking `&name` gives the address of
 * the corresponding marker; the "array" type is only there to make
 * `&name` return `uintptr_t` cleanly and to allow `sizeof` tricks.
 *
 * RULES
 * -----
 *   - Every `extern` declaration of a linker symbol lives HERE,
 *     nowhere else.
 *   - Every definition lives in `mk/hajlibc.ld`, nowhere else.
 *   - Users include this header; they never re-declare the symbols.
 *
 * The symbols are split by section so it is easy to see which range
 * each one belongs to.
 */

#ifndef HAJ_BITS_SYMBOLS_H
# define HAJ_BITS_SYMBOLS_H

# include <stdint.h>

/* ----- .text (executable code) ----- */

extern char	__text_start[];		/**< first byte of .text */
extern char	__text_end[];		/**< one past the last byte of .text */

/* ----- .data (initialised writable data) ----- */

extern char	__data_start[];		/**< first byte of .data */
extern char	_edata[];			/**< one past the last byte of .data */

/* ----- .bss (zero-initialised writable data) ----- */

extern char	__bss_start[];		/**< first byte of .bss */
extern char	__bss_end[];		/**< one past the last byte of .bss */

/* ----- TLS template (.tdata + .tbss) ----- */

extern char	__tdata_start[];	/**< start of the .tdata image */
extern char	__tdata_end[];		/**< end of the .tdata image */
extern char	__tbss_start[];		/**< start of the .tbss template */
extern char	__tbss_end[];		/**< end of the .tbss template */

/*
 * TLS metadata, as absolute *values* (not addresses). Declared as
 * `char[]` so that `(size_t)__tls_size` yields the value; the linker
 * script uses `ABSOLUTE(...)` to make them numbers, not addresses.
 */
extern char	__tls_size[];		/**< bytes per thread's TLS block */
extern char	__tls_align[];		/**< alignment of the TLS block */

/* ----- Constructor / destructor arrays ----- */

extern void	(*__preinit_array_start[])(void);
extern void	(*__preinit_array_end[])(void);
extern void	(*__init_array_start[])(void);
extern void	(*__init_array_end[])(void);
extern void	(*__fini_array_start[])(void);
extern void	(*__fini_array_end[])(void);

extern uintptr_t __haj_initial_sp;	/**< initial stack pointer, set by start.c */

#endif /* HAJ_BITS_SYMBOLS_H */
