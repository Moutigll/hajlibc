/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file auxv.h
 * @brief Definition of the auxiliary vector types and getauxval().
 * @Created: 2026/09/28 11:18:08 by Moutig
 * @Updated: 2026/09/28 12:02:03 by Moutig
 *
 * The auxiliary vector is a set of (type, value) pairs placed on
 * the initial stack by the kernel at program startup. It carries
 * information about the hardware, the kernel, and the process
 * that cannot be obtained otherwise without a syscall.
 *
 * getauxval() searches this vector for a given type. It is not
 * specified by POSIX; it comes from the Linux/glibc world and is
 * also available on FreeBSD and other ELF platforms.
 */

#ifndef _SYS_AUXV_H
# define _SYS_AUXV_H

# ifdef __cplusplus
extern "C" {
# endif

/**
 * @brief Pointer to the auxiliary vector, captured by _start.
 */
extern const unsigned long *__haj_auxv;

/* ----- Auxiliary vector types ----- */
/**
 * The numeric values are part of the ELF ABI and are stable
 * across all ELF platforms. They come from <elf.h>.
 */

# define AT_NULL			0	/* End of vector. */
# define AT_IGNORE			1	/* Entry should be ignored. */
# define AT_EXECFD			2	/* File descriptor of program. */
# define AT_PHDR			3	/* Program headers for program. */
# define AT_PHENT			4	/* Size of a program header entry. */
# define AT_PHNUM			5	/* Number of program headers. */
# define AT_PAGESZ			6	/* System page size. */
# define AT_BASE			7	/* Base address of interpreter. */
# define AT_FLAGS			8	/* Flags. */
# define AT_ENTRY			9	/* Entry point of program. */
# define AT_NOTELF			10	/* Program is not ELF. */
# define AT_UID				11	/* Real uid. */
# define AT_EUID			12	/* Effective uid. */
# define AT_GID				13	/* Real gid. */
# define AT_EGID			14	/* Effective gid. */
# define AT_PLATFORM		15	/* String identifying CPU. */
# define AT_HWCAP			16	/* CPU capabilities. */
# define AT_CLKTCK			17	/* Frequency of times(). */
# define AT_SECURE			23	/* Secure mode boolean. */
# define AT_BASE_PLATFORM	24	/* String identifying platform. */
# define AT_RANDOM			25	/* Address of 16 random bytes. */
# define AT_HWCAP2			26	/* More CPU capabilities. */
# define AT_EXECFN			31	/* Filename of executable. */
# define AT_SYSINFO			32	/* Entry point to the vDSO (i386 only). */
# define AT_SYSINFO_EHDR	33	/* Base of the vDSO. */

/**
 * @brief Return the value associated with an auxiliary vector type.
 *
 * @param type One of the AT_* constants.
 * @return The value associated with type, or 0 if type is not
 *         present in the auxiliary vector.
 *
 * Note: 0 is a valid value for some types (AT_UID, AT_GID, ...),
 * so the return value cannot always distinguish "not found" from
 * "value is zero". The caller must know which types may be zero.
 */
unsigned long getauxval(unsigned long type);

# ifdef __cplusplus
}
# endif

#endif /* _SYS_AUXV_H */
