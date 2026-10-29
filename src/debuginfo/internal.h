/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file internal.h
 * @brief Internal header for debuginfo module.
 * @Created: 2026/10/28 08:54:53 by Moutig
 * @Updated: 2026/10/28 12:48:44 by Moutig
 *
 * This header file defines internal structures, constants, and function prototypes
 * used by the debuginfo module. It is not intended for public use and should only
 * be included by source files within the debuginfo module.
 */

#ifndef HAJ_DI_INTERNAL_H
# define HAJ_DI_INTERNAL_H

# include <elf.h>
# include <debuginfo.h>

/* Default chunk size. One mmap is done per chunk. */
# define DI_CHUNK_SIZE	(64u * 1024u)

/* Max chunks per object. 256 * 64 KiB = 16 MiB of symbol tables,
 * which is far above anything a normal binary produces. */
# define DI_MAX_CHUNKS	256

/**
 * @brief Structure representing a function in the debug information.
 *
 * This structure holds information about a function, including its address range and name.
 */
typedef struct {
	uintptr_t	lo;
	uintptr_t	hi;
	const char	*name;
} di_func;

/**
 * @brief Structure representing a line entry in the debug information.
 *
 * This structure holds information about a source line, including its address,
 * file name, line number, and sequence information.
 */
typedef struct {
	uintptr_t	addr;
	const char	*file;
	uint32_t	line;
	uint32_t	seq;
	uint8_t		endSequence;
} di_lne;

/**
 * @brief Structure representing a debug information object.
 *
 * This structure holds information about the debug information associated with an ELF file,
 * including function symbols, source lines, and memory management data.
 */
struct s_di_object {
	s_elf		*elf;
	char		pathBuff[256];

	/* Function symbols, address-sorted. */
	di_func		*funcs;
	size_t		nfuncs;

	/* Source lines, address-sorted. */
	di_lne		*lines;
	size_t		nlines;

	/* Chunk allocator. */
	void		*chunks[DI_MAX_CHUNKS];
	size_t		chunkSizes[DI_MAX_CHUNKS];
	unsigned	nchunks;
	uint8_t		*cur;
	size_t		left;

	/* Diagnostics. Fixed buffers, no allocation. */
	char		err[256];
	char		warn[256];

	int			ready;
};



/**
 *@brief Allocate memory for a debug information object.
 *
 * This function allocates memory for a new debug information object and initializes it.
 *
 * @return Pointer to the newly allocated di_object, or NULL on failure.
 */
void	*di_arena_alloc(di_object *d, size_t n);

/**
 *@brief Release memory associated with a debug information object.
 *
 * This function releases all memory associated with the specified debug information object.
 *
 * @param d Pointer to the di_object to release.
 */
void	di_arena_release(di_object *d);


/**
 *@brief Duplicate a string using the debug information object's memory arena.
 *
 * This function duplicates the specified string using the memory arena of the debug information object.
 *
 * @param d Pointer to the di_object whose arena will be used for allocation.
 * @param s The string to duplicate.
 * @return Pointer to the duplicated string, or NULL on failure.
 */
const char *di_arena_strdup(di_object *d, const char *s);

/**
 *@brief Join a directory and file name into a single path using the debug information object's memory arena.
 *
 * This function joins the specified directory and file name into a single path string,
 * using the memory arena of the debug information object for allocation.
 *
 * @param d Pointer to the di_object whose arena will be used for allocation.
 * @param dir The directory path.
 * @param name The file name.
 * @return Pointer to the joined path string, or NULL on failure.
 */
const char *di_path_join(di_object *d, const char *dir, const char *name);

/**
 * @brief Read an unsigned LEB128 value from a byte stream.
 *
 * This function reads an unsigned LEB128 value from the specified byte stream,
 * updating the pointer to the current position in the stream.
 *
 * @param pp Pointer to the pointer to the current position in the byte stream.
 * @param end Pointer to the end of the byte stream.
 * @return The decoded unsigned LEB128 value.
 */
static __HAJ_INLINE uint64_t di_uleb(const uint8_t **pp, const uint8_t *end)
{
	uint64_t v = 0;
	unsigned s = 0;

	while (*pp < end) {
		uint8_t b = *(*pp)++;
		if (s < 64) v |= (uint64_t)(b & 0x7F) << s;
		if (!(b & 0x80)) break;
		s += 7;
	}
	return (v);
}

/**
 * @brief Read a signed LEB128 value from a byte stream.
 *
 * This function reads a signed LEB128 value from the specified byte stream,
 * updating the pointer to the current position in the stream.
 *
 * @param pp Pointer to the pointer to the current position in the byte stream.
 * @param end Pointer to the end of the byte stream.
 * @return The decoded signed LEB128 value.
 */
static __HAJ_INLINE int64_t di_sleb(const uint8_t **pp, const uint8_t *end)
{
	int64_t		v = 0;
	unsigned	s = 0;
	uint8_t		b = 0;

	while (*pp < end) {
		b = *(*pp)++;
		if (s < 64) v |= (int64_t)(b & 0x7F) << s;
		s += 7;
		if (!(b & 0x80)) break;
	}
	if (s < 64 && (b & 0x40)) v |= -((int64_t)1 << s);
	return (v);
}

/**
 * @brief Read a value of a specified size from a byte stream.
 *
 * This function reads a value of the specified size from the byte stream,
 * updating the pointer to the current position in the stream.
 *
 * @param pp Pointer to the pointer to the current position in the byte stream.
 * @param end Pointer to the end of the byte stream.
 * @param n The size of the value to read (in bytes).
 * @param out Pointer to the location where the read value will be stored.
 * @return 0 on success, -1 on failure.
 */
static __HAJ_INLINE int di_rd(const uint8_t **pp, const uint8_t *end, unsigned n, uint64_t *out)
{
	uint64_t v = 0;
	if (n > 8 || (size_t)(end - *pp) < n) return -1;
	for (unsigned i = 0; i < n; i++)
		v |= (uint64_t)(*pp)[i] << (8 * i);
	*pp += n;
	*out = v;
	return (0);
}

/**
 * @brief Callback type for iterating over line entries in the debug information.
 *
 * This callback is invoked for each line entry found during the iteration.
 * It provides the address, file name, line number, and sequence information
 * for each line entry.
 *
 * @param addr The address of the line entry.
 * @param file The file name associated with the line entry.
 * @param line The line number associated with the line entry.
 * @param endSequence Flag indicating if this is the end of a sequence.
 * @param arg User-defined argument passed to the callback.
 * @return 0 on success, -1 on failure.
 */
typedef int (*di_line_cb)(uintptr_t addr, const char *file, uint32_t line, int endSequence, void *arg);

/**
 * @brief Parse line entries from the debug information and invoke a callback for each entry.
 *
 * This function parses the line entries from the debug information associated with the
 * specified debug information object. For each line entry found, it invokes the provided
 * callback function with the relevant information.
 *
 * @param d Pointer to the di_object containing the debug information.
 * @param cb The callback function to invoke for each line entry.
 * @param arg User-defined argument to pass to the callback function.
 * @return 0 on success, -1 on failure.
 */
int di_dwarf_parse_line(di_object *d, di_line_cb cb, void *arg);

#endif /* HAJ_DI_INTERNAL_H */
