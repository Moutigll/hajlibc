/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file link.h
 * @brief Linker symbols and dl_iterate_phdr interface for hajlibc.
 * @Created: 2026/10/27 08:25:42 by Moutig
 * @Updated: 2026/10/29 01:02:50 by Moutig
 *
 * This header defines the interface for iterating over loaded ELF objects
 * using dl_iterate_phdr, as well as linker symbols for .text, .data, .bss, and TLS sections.
 * It provides a way to access information about loaded ELF objects
 * and their program headers in a platform-independent manner.
 */

#ifndef HAJ_LINK_H
# define HAJ_LINK_H

# include <stdint.h>
# include <elf.h>

# ifdef __cplusplus
extern "C" {
# endif

/**
 * @brief Information about a loaded ELF object.
 *
 * This structure is passed to the callback function provided to dl_iterate_phdr.
 * It contains the base address of the loaded object, its name, and pointers to its program headers.
 */
struct dl_phdr_info {
	Elf64_Addr			dlpi_addr;
	const char			*dlpi_name;
	const Elf64_Phdr	*dlpi_phdr;
	Elf64_Half			dlpi_phnum;
	unsigned long long	dlpi_adds;
	unsigned long long	dlpi_subs;
	size_t				dlpi_tls_modid;
	void				*dlpi_tls_data;
};

/**
 * @brief Iterate over loaded ELF objects and invoke a callback for each.
 *
 * This function iterates over all loaded ELF objects in the current process,
 * invoking the provided callback function for each object. The callback receives
 * a pointer to a dl_phdr_info structure containing information about the object.
 *
 * @param callback The callback function to invoke for each loaded object.
 * @param data A pointer to user-defined data that will be passed to the callback.
 * @return The return value of the last callback invoked, or 0 if no callbacks were invoked.
 */
int dl_iterate_phdr(int (*callback)(struct dl_phdr_info *, size_t, void *), void *data);

# ifdef __cplusplus
}
# endif

#endif /* HAJ_LINK_H */
