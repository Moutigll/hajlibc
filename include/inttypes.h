/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file inttypes.h
 * @brief Format conversion of integer types.
 * @Created: 2026/11/19 16:53:58 by Moutig
 * @Updated: 2026/11/19 18:16:43 by Moutig
 *
 * This header includes <stdint.h> and adds:
 *   - the PRI* macros, printf conversion specifiers for the <stdint.h>
 *     types (e.g. printf("%" PRId64 "\n", x));
 *   - the SCN* macros, the scanf equivalents;
 *   - imaxdiv_t and the greatest-width integer functions.
 *
 */

#ifndef _INTTYPES_H
# define _INTTYPES_H

# include <bits/wordsize.h>
# include <stdint.h>

/* ----- Types ----- */

/**
 * @brief Result of imaxdiv(): quotient and remainder.
 */
typedef struct {
	intmax_t	quot;	/* Quotient */
	intmax_t	rem;	/* Remainder */
}	imaxdiv_t;

/* ----- Length modifiers (internal) ----- */

/* int8_t / int16_t: promoted to int by printf, exact for scanf. */
# define __HAJ_PRI8		"hh"
# define __HAJ_PRI16	"h"
# define __HAJ_PRI32	""

/* int64_t: long on LP64, long long otherwise (see <bits/types.h>). */
# if __HAJ_SIZEOF_LONG == 8
#  define __HAJ_PRI64	"l"
# else
#  define __HAJ_PRI64	"ll"
# endif

/* intptr_t: int64_t or int32_t, by pointer size. */
# if __HAJ_SIZEOF_POINTER == 8
#  define __HAJ_PRIPTR	__HAJ_PRI64
# else
#  define __HAJ_PRIPTR	__HAJ_PRI32
# endif

/* int_fast8/16/32_t: int64_t on 64-bit platforms, int32_t otherwise. */
# if __HAJ_WORDSIZE == 64
#  define __HAJ_PRIFAST	__HAJ_PRI64
# else
#  define __HAJ_PRIFAST	__HAJ_PRI32
# endif

/* intmax_t: int64_t. */
# define __HAJ_PRIMAX	__HAJ_PRI64

/* ----- printf: signed decimal (d) ----- */

# define PRId8			"d"
# define PRId16			"d"
# define PRId32			"d"
# define PRId64			__HAJ_PRI64 "d"

# define PRIdLEAST8		PRId8
# define PRIdLEAST16	PRId16
# define PRIdLEAST32	PRId32
# define PRIdLEAST64	PRId64

# define PRIdFAST8		__HAJ_PRIFAST "d"
# define PRIdFAST16		__HAJ_PRIFAST "d"
# define PRIdFAST32		__HAJ_PRIFAST "d"
# define PRIdFAST64		PRId64

# define PRIdMAX		__HAJ_PRIMAX "d"
# define PRIdPTR		__HAJ_PRIPTR "d"

/* ----- printf: signed decimal (i) ----- */

# define PRIi8			"i"
# define PRIi16			"i"
# define PRIi32			"i"
# define PRIi64			__HAJ_PRI64 "i"

# define PRIiLEAST8		PRIi8
# define PRIiLEAST16	PRIi16
# define PRIiLEAST32	PRIi32
# define PRIiLEAST64	PRIi64

# define PRIiFAST8		__HAJ_PRIFAST "i"
# define PRIiFAST16		__HAJ_PRIFAST "i"
# define PRIiFAST32		__HAJ_PRIFAST "i"
# define PRIiFAST64		PRIi64

# define PRIiMAX		__HAJ_PRIMAX "i"
# define PRIiPTR		__HAJ_PRIPTR "i"

/* ----- printf: unsigned octal (o) ----- */

# define PRIo8			"o"
# define PRIo16			"o"
# define PRIo32			"o"
# define PRIo64			__HAJ_PRI64 "o"

# define PRIoLEAST8		PRIo8
# define PRIoLEAST16	PRIo16
# define PRIoLEAST32	PRIo32
# define PRIoLEAST64	PRIo64

# define PRIoFAST8		__HAJ_PRIFAST "o"
# define PRIoFAST16		__HAJ_PRIFAST "o"
# define PRIoFAST32		__HAJ_PRIFAST "o"
# define PRIoFAST64		PRIo64

# define PRIoMAX		__HAJ_PRIMAX "o"
# define PRIoPTR		__HAJ_PRIPTR "o"

/* ----- printf: unsigned decimal (u) ----- */

# define PRIu8			"u"
# define PRIu16			"u"
# define PRIu32			"u"
# define PRIu64			__HAJ_PRI64 "u"

# define PRIuLEAST8		PRIu8
# define PRIuLEAST16	PRIu16
# define PRIuLEAST32	PRIu32
# define PRIuLEAST64	PRIu64

# define PRIuFAST8		__HAJ_PRIFAST "u"
# define PRIuFAST16		__HAJ_PRIFAST "u"
# define PRIuFAST32		__HAJ_PRIFAST "u"
# define PRIuFAST64		PRIu64

# define PRIuMAX		__HAJ_PRIMAX "u"
# define PRIuPTR		__HAJ_PRIPTR "u"

/* ----- printf: unsigned hexadecimal (x) ----- */

# define PRIx8			"x"
# define PRIx16			"x"
# define PRIx32			"x"
# define PRIx64			__HAJ_PRI64 "x"

# define PRIxLEAST8		PRIx8
# define PRIxLEAST16	PRIx16
# define PRIxLEAST32	PRIx32
# define PRIxLEAST64	PRIx64

# define PRIxFAST8		__HAJ_PRIFAST "x"
# define PRIxFAST16		__HAJ_PRIFAST "x"
# define PRIxFAST32		__HAJ_PRIFAST "x"
# define PRIxFAST64		PRIx64

# define PRIxMAX		__HAJ_PRIMAX "x"
# define PRIxPTR		__HAJ_PRIPTR "x"

/* ----- printf: unsigned hexadecimal, upper case (X) ----- */

# define PRIX8			"X"
# define PRIX16			"X"
# define PRIX32			"X"
# define PRIX64			__HAJ_PRI64 "X"

# define PRIXLEAST8		PRIX8
# define PRIXLEAST16	PRIX16
# define PRIXLEAST32	PRIX32
# define PRIXLEAST64	PRIX64

# define PRIXFAST8		__HAJ_PRIFAST "X"
# define PRIXFAST16		__HAJ_PRIFAST "X"
# define PRIXFAST32		__HAJ_PRIFAST "X"
# define PRIXFAST64		PRIX64

# define PRIXMAX		__HAJ_PRIMAX "X"
# define PRIXPTR		__HAJ_PRIPTR "X"

/*
 * scanf writes through a pointer, so unlike printf the length modifier
 * must match the exact width, 8 and 16 bits included.
 */

/* ----- scanf: signed decimal (d) ----- */

# define SCNd8			__HAJ_PRI8 "d"
# define SCNd16			__HAJ_PRI16 "d"
# define SCNd32			__HAJ_PRI32 "d"
# define SCNd64			__HAJ_PRI64 "d"

# define SCNdLEAST8		SCNd8
# define SCNdLEAST16	SCNd16
# define SCNdLEAST32	SCNd32
# define SCNdLEAST64	SCNd64

# define SCNdFAST8		__HAJ_PRIFAST "d"
# define SCNdFAST16		__HAJ_PRIFAST "d"
# define SCNdFAST32		__HAJ_PRIFAST "d"
# define SCNdFAST64		SCNd64

# define SCNdMAX		__HAJ_PRIMAX "d"
# define SCNdPTR		__HAJ_PRIPTR "d"

/* ----- scanf: signed integer, base from prefix (i) ----- */

# define SCNi8			__HAJ_PRI8 "i"
# define SCNi16			__HAJ_PRI16 "i"
# define SCNi32			__HAJ_PRI32 "i"
# define SCNi64			__HAJ_PRI64 "i"

# define SCNiLEAST8		SCNi8
# define SCNiLEAST16	SCNi16
# define SCNiLEAST32	SCNi32
# define SCNiLEAST64	SCNi64

# define SCNiFAST8		__HAJ_PRIFAST "i"
# define SCNiFAST16		__HAJ_PRIFAST "i"
# define SCNiFAST32		__HAJ_PRIFAST "i"
# define SCNiFAST64		SCNi64

# define SCNiMAX		__HAJ_PRIMAX "i"
# define SCNiPTR		__HAJ_PRIPTR "i"

/* ----- scanf: unsigned octal (o) ----- */

# define SCNo8			__HAJ_PRI8 "o"
# define SCNo16			__HAJ_PRI16 "o"
# define SCNo32			__HAJ_PRI32 "o"
# define SCNo64			__HAJ_PRI64 "o"

# define SCNoLEAST8		SCNo8
# define SCNoLEAST16	SCNo16
# define SCNoLEAST32	SCNo32
# define SCNoLEAST64	SCNo64

# define SCNoFAST8		__HAJ_PRIFAST "o"
# define SCNoFAST16		__HAJ_PRIFAST "o"
# define SCNoFAST32		__HAJ_PRIFAST "o"
# define SCNoFAST64		SCNo64

# define SCNoMAX		__HAJ_PRIMAX "o"
# define SCNoPTR		__HAJ_PRIPTR "o"

/* ----- scanf: unsigned decimal (u) ----- */

# define SCNu8			__HAJ_PRI8 "u"
# define SCNu16			__HAJ_PRI16 "u"
# define SCNu32			__HAJ_PRI32 "u"
# define SCNu64			__HAJ_PRI64 "u"

# define SCNuLEAST8		SCNu8
# define SCNuLEAST16	SCNu16
# define SCNuLEAST32	SCNu32
# define SCNuLEAST64	SCNu64

# define SCNuFAST8		__HAJ_PRIFAST "u"
# define SCNuFAST16		__HAJ_PRIFAST "u"
# define SCNuFAST32		__HAJ_PRIFAST "u"
# define SCNuFAST64		SCNu64

# define SCNuMAX		__HAJ_PRIMAX "u"
# define SCNuPTR		__HAJ_PRIPTR "u"

/* ----- scanf: unsigned hexadecimal (x) ----- */

# define SCNx8			__HAJ_PRI8 "x"
# define SCNx16			__HAJ_PRI16 "x"
# define SCNx32			__HAJ_PRI32 "x"
# define SCNx64			__HAJ_PRI64 "x"

# define SCNxLEAST8		SCNx8
# define SCNxLEAST16	SCNx16
# define SCNxLEAST32	SCNx32
# define SCNxLEAST64	SCNx64

# define SCNxFAST8		__HAJ_PRIFAST "x"
# define SCNxFAST16		__HAJ_PRIFAST "x"
# define SCNxFAST32		__HAJ_PRIFAST "x"
# define SCNxFAST64		SCNx64

# define SCNxMAX		__HAJ_PRIMAX "x"
# define SCNxPTR		__HAJ_PRIPTR "x"

/* ----- Greatest-width integer functions ----- */

/**
 * @brief Absolute value of an intmax_t.
 *
 * @param j Value.
 * @return |j|. Undefined if j is INTMAX_MIN (not representable).
 */
intmax_t	imaxabs(intmax_t j);

/**
 * @brief Quotient and remainder of an intmax_t division.
 *
 * The quotient is truncated toward zero, and numer == quot * denom + rem.
 *
 * @param numer Numerator.
 * @param denom Denominator (not 0).
 * @return The quotient and remainder.
 */
imaxdiv_t	imaxdiv(intmax_t numer, intmax_t denom);

/**
 * @brief Convert the start of a string to an intmax_t.
 *
 * Same rules as strtol() (see <stdlib.h>).
 *
 * @param nptr   String to convert.
 * @param endptr If not NULL, receives the address of the first character
 *               not converted, or `nptr` if no digit was read.
 * @param base   0, or 2 to 36.
 * @return The converted value; INTMAX_MIN or INTMAX_MAX with
 *         errno = ERANGE if out of range.
 */
intmax_t	strtoimax(const char *restrict nptr, char **restrict endptr, int base);

/**
 * @brief Convert the start of a string to an uintmax_t.
 *
 * Same rules as strtoul() (see <stdlib.h>).
 *
 * @param nptr   String to convert.
 * @param endptr If not NULL, receives the address of the first character
 *               not converted, or `nptr` if no digit was read.
 * @param base   0, or 2 to 36.
 * @return The converted value; UINTMAX_MAX with errno = ERANGE if out of
 *         range.
 */
uintmax_t	strtoumax(const char *restrict nptr, char **restrict endptr, int base);

#endif /* _INTTYPES_H */
