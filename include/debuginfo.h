/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file debuginfo.h
 * @brief Debug information lookup interface.
 * @Created: 2026/10/28 08:33:37 by Moutig
 * @Updated: 2026/10/29 00:36:45 by Moutig
 *
 * This header defines the interface for looking up debug information
 * (function names, source file paths, and line numbers) for given
 * program counter addresses. It provides functions to initialize the
 * debug information subsystem, perform lookups, and retrieve error
 * messages.
 */

#ifndef HAJ_DEBUGINFO_H
# define HAJ_DEBUGINFO_H

# include <stdint.h>

# ifdef __cplusplus
extern "C" {
# endif


typedef struct s_di_object di_object;

/**
 * @brief Symbol types.
 *
 * This enumeration defines the types of symbols that can be found in
 * an ELF file, including functions, objects, TLS variables, sections,
 * and files.
 */
typedef enum {
	DI_SYM_UNKNOWN	= 0,
	DI_SYM_FUNCTION	= 1,
	DI_SYM_OBJECT	= 2,
	DI_SYM_TLS		= 3,
	DI_SYM_SECTION	= 4,
	DI_SYM_FILE		= 5,
} di_sym_type;

/**
 * @brief Symbol information structure.
 *
 * This structure holds information about a symbol, including its name,
 * address, size, type, and the section index it belongs to.
 */
typedef struct {
	const char	*name;
	uintptr_t	value;
	size_t		size;
	di_sym_type	type;
	unsigned	section;
} di_symbol;

/**
 * @brief Section information structure.
 *
 * This structure holds information about a section in an ELF file,
 * including its name, type, address, offset, and size.
 */
typedef struct {
	const char	*name;
	uint32_t	type;
	uint64_t	addr;
	uint64_t	offset;
	uint64_t	size;
} di_section;

/**
 * @brief Source information structure.
 *
 * This structure holds information about a source code location,
 * including the file path, file name, and line number.
 */
typedef struct {
	const char	*path;	/* directory + file, may be relative */
	const char	*file;	/* basename of path */
	uint32_t	line;	/* 1-based, 0 if unknown */
} di_source;

/**
 * @brief Debug information structure.
 *
 * This structure holds information about a debug entry, including the
 * function name, function address, source file path, source file name,
 * and line number.
 */
typedef struct {
	const char	*func;
	uintptr_t	func_lo;
	const char	*path;
	const char	*file;
	uint32_t	line;
} di_info;


/**
 * @brief Callback type for enumerating loaded ELF files.
 *
 * This callback is invoked for each ELF file currently mapped in the
 * calling process. The callback receives the path, base address, and
 * mapped size of the ELF file, along with a user-defined argument.
 *
 * @param path The path to the ELF file.
 * @param base The base address where the ELF file is mapped.
 * @param mapped_size The size of the mapped region of the ELF file.
 * @param arg User-defined argument passed to the callback.
 */
typedef void (*di_loaded_cb)(const char *path, uintptr_t base, size_t mapped_size, void *arg);



/**
 * @brief Open a debug information object from an ELF file.
 *
 * This function opens an ELF file and loads its debug information,
 * returning a handle to the debug information object. The caller is
 * responsible for closing the object with `di_object_close()` when done.
 *
 * @param path The path to the ELF file.
 * @return A pointer to the debug information object, or NULL on error.
 */
di_object	*di_object_open(const char *path);

/**
 * @brief Open a debug information object for the current process.
 *
 * This function opens the debug information for the currently running
 * process, returning a handle to the debug information object. The
 * caller is responsible for closing the object with `di_object_close()`
 * when done.
 *
 * @return A pointer to the debug information object, or NULL on error.
 */
di_object	*di_object_open_self(void);

/**
 * @brief Close a debug information object.
 *
 * This function closes a previously opened debug information object
 * and releases any associated resources.
 *
 * @param d The debug information object to close.
 */
void		di_object_close(di_object *d);



/**
 * @brief Get the path of the ELF file associated with the debug information object.
 *
 * This function returns the path of the ELF file from which the debug
 * information was loaded. The returned string is valid until the object
 * is closed.
 *
 * @param d The debug information object.
 * @return The path of the ELF file, or NULL if unavailable.
 */
const char	*di_object_path(const di_object *d);

/**
 * @brief Get the size of the debug information object.
 *
 * This function returns the size of the debug information object in bytes.
 *
 * @param d The debug information object.
 * @return The size of the debug information object in bytes.
 */
size_t		di_object_size(const di_object *d);

/**
 * @brief Get the last error message for the debug information object.
 *
 * This function returns a string describing the last error that occurred
 * while working with the debug information object. The returned string is
 * valid until the next call to any function that modifies the object's state.
 *
 * @param d The debug information object.
 * @return A string describing the last error, or NULL if no error occurred.
 */
const char	*di_object_error(const di_object *d);

/**
 * @brief Get the last warning message for the debug information object.
 *
 * This function returns a string describing the last warning that occurred
 * while working with the debug information object. The returned string is
 * valid until the next call to any function that modifies the object's state.
 *
 * @param d The debug information object.
 * @return A string describing the last warning, or NULL if no warning occurred.
 */
const char	*di_object_warning(const di_object *d);



/**
 * @brief Find a function by address.
 *
 * This function finds the function whose [value, value+size) contains `addr`.
 * Symbols with size 0 are extended to the next function's start.
 * Returns 0 on hit, -1 if no function contains `addr`.
 *
 * @param d The debug information object.
 * @param addr The address to search for.
 * @param out A pointer to a di_symbol structure to fill in.
 * @return 0 on success, -1 on failure.
 */
int di_object_find_function(di_object *d, uintptr_t addr, di_symbol *out);

/**
 * @brief Find a symbol by name.
 *
 * This function finds a symbol with the specified name in the debug
 * information object. If found, it fills in the provided di_symbol
 * structure with the symbol's information.
 *
 * @param d The debug information object.
 * @param name The name of the symbol to find.
 * @param out A pointer to a di_symbol structure to fill in.
 * @return 0 on success, -1 if the symbol was not found.
 */
int di_object_find_symbol(di_object *d, const char *name, di_symbol *out);

/**
 * @brief Iterate over all symbols in the debug information object.
 *
 * This function iterates over all symbols in the debug information
 * object, calling the provided callback function for each symbol. The
 * iteration stops if the callback returns a non-zero value.
 *
 * @param d The debug information object.
 * @param cb The callback function to call for each symbol.
 * @param arg A user-defined argument to pass to the callback function.
 * @return 0 on success, -1 on error.
 */
int di_object_foreach_symbol(di_object *d, int (*cb)(const di_symbol *, void *), void *arg);



/**
 * @brief Find the source information for a given address.
 *
 * This function finds the source file and line number corresponding
 * to the specified address in the debug information object. If found,
 * it fills in the provided di_source structure with the source
 * information.
 *
 * @param d The debug information object.
 * @param addr The address to search for.
 * @param out A pointer to a di_source structure to fill in.
 * @return 0 on success, -1 if no source information was found for the address.
 */
int di_object_find_source(di_object *d, uintptr_t addr, di_source *out);

/**
 * @brief Iterate over all source information entries in the debug information object.
 *
 * This function iterates over all source information entries in the
 * debug information object, calling the provided callback function for
 * each entry. The iteration stops if the callback returns a non-zero value.
 *
 * @param d The debug information object.
 * @param cb The callback function to call for each source entry.
 * @param arg A user-defined argument to pass to the callback function.
 * @return 0 on success, -1 on error.
 */
int di_object_foreach_source(di_object	*d,
							 int		(*cb)(uintptr_t addr, const di_source *src, void *arg),
							 void		*arg);

/**
 * @brief Iterate over all sections in the debug information object.
 *
 * This function iterates over all sections in the debug information
 * object, calling the provided callback function for each section. The
 * iteration stops if the callback returns a non-zero value.
 *
 * @param d The debug information object.
 * @param cb The callback function to call for each section.
 * @param arg A user-defined argument to pass to the callback function.
 * @return 0 on success, -1 on error.
 */
int	di_object_foreach_section(di_object	*d,
							  int		(*cb)(const di_section *, void *),
							  void		*arg);

/**
 * @brief Get the data of a section by name.
 *
 * This function retrieves the data of a section with the specified
 * name from the debug information object. If found, it returns a
 * pointer to the section data and fills in the size of the section.
 *
 * @param d The debug information object.
 * @param name The name of the section to retrieve.
 * @param outSize A pointer to a variable to receive the size of the section data.
 * @return A pointer to the section data, or NULL if the section was not found.
 */
const void *di_object_section_data(di_object *d, const char *name, size_t *outSize);


/**
 * @brief Enumerate all loaded ELF files in the current process.
 *
 * This function enumerates all ELF files currently mapped in the
 * calling process, invoking the specified callback for each file.
 *
 * @param cb The callback function to invoke for each loaded ELF file.
 * @param arg User-defined argument to pass to the callback function.
 * @return 0 on success, -1 on error.
 */
int di_enumerate_loaded(di_loaded_cb cb, void *arg);


/**
 * @brief Initialize the debug information subsystem.
 *
 * This function initializes the debug information subsystem, preparing
 * it for use. It must be called before any other functions in this
 * interface are used.
 *
 * @return 0 on success, -1 on error.
 */
int		di_init(void);

/**
 * @brief Finalize the debug information subsystem.
 *
 * This function finalizes the debug information subsystem, releasing
 * any resources that were allocated during initialization or use.
 * It should be called when the debug information functionality is no
 * longer needed.
 */
void	di_fini(void);

/**
 * @brief Lookup debug information for a given address.
 *
 * This function looks up the debug information (function name, source
 * file, and line number) corresponding to the specified address. If
 * found, it fills in the provided di_info structure with the relevant
 * information.
 *
 * @param addr The address to look up.
 * @param out A pointer to a di_info structure to fill in with the debug information.
 * @return 0 on success, -1 if no debug information was found for the address.
 */
int		di_lookup(uintptr_t addr, di_info *out);



/**
 * @brief Get the last error message from the debug information subsystem.
 *
 * This function returns a string describing the last error that occurred
 * in the debug information subsystem. The returned string is valid until
 * the next call to any function that modifies the subsystem's state.
 *
 * @return A string describing the last error, or NULL if no error occurred.
 */
const char	*di_error(void);

/**
 * @brief Get the last warning message from the debug information subsystem.
 *
 * This function returns a string describing the last warning that occurred
 * in the debug information subsystem. The returned string is valid until
 * the next call to any function that modifies the subsystem's state.
 *
 * @return A string describing the last warning, or NULL if no warning occurred.
 */
const char	*di_warning(void);

/**
 * @brief Get the counts of functions and lines in the debug information subsystem.
 *
 * This function retrieves the total number of functions and lines
 * currently loaded in the debug information subsystem. The counts are
 * returned through the provided pointers.
 *
 * @param nfuncs Pointer to a variable to receive the number of functions (can be NULL).
 * @param nlines Pointer to a variable to receive the number of lines (can be NULL).
 */
void		di_counts(size_t *nfuncs, size_t *nlines);

# ifdef __cplusplus
}
# endif

#endif
