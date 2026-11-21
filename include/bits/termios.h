/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file termios.h
 * @brief Terminal interface structure and constants.
 * @Created: 2026/11/22 00:11:43 by Moutig
 * @Updated: 2026/11/22 00:13:07 by Moutig
 *
 * Defines struct termios and the TCGETS / TCSETS ioctl requests,
 * matching the Linux kernel ABI on x86_64 and aarch64. The layout
 * must not change: it is passed directly to the kernel through
 * ioctl(2), and a mismatch would corrupt the caller's structure.
 *
 * Only the fields needed by isatty() (TCGETS) and the future
 * tcgetattr / tcsetattr are provided. The full set of c_iflag /
 * c_oflag / c_cflag / c_lflag bits will be added with those
 * functions.
 */

#ifndef _BITS_TERMIOS_H
# define _BITS_TERMIOS_H

/** Number of control characters in c_cc[]. */
# define NCCS 32

/**
 * @brief Terminal attributes.
 *
 * Mirrors the kernel's struct termios for the asm-generic ABI,
 * used by x86_64 and aarch64 Linux. Do not reorder the fields.
 */
struct termios
{
	unsigned int	c_iflag;	/* Input flags. */
	unsigned int	c_oflag;	/* Output flags. */
	unsigned int	c_cflag;	/* Control flags. */
	unsigned int	c_lflag;	/* Local flags. */
	unsigned char	c_line;		/* Line discipline. */
	unsigned char	c_cc[NCCS];	/* Control characters. */
	unsigned int	c_ispeed;	/* Input baud rate. */
	unsigned int	c_ospeed;	/* Output baud rate. */
};

/* ----- ioctl requests ----- */

/** Get the terminal attributes into a struct termios. */
# define TCGETS 0x5401

/** Set the terminal attributes from a struct termios. */
# define TCSETS 0x5402

/** Set the terminal attributes, leaving output unchanged. */
# define TCSETSW 0x5403

/** Set the terminal attributes, leaving output and input unchanged. */
# define TCSETSF 0x5404

#endif /* _BITS_TERMIOS_H */
