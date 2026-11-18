/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file elf.h
 * @brief ELF64 header and section definitions, and loaded ELF handle.
 * @Created: 2026/10/28 08:11:18 by Moutig
 * @Updated: 2026/10/29 05:12:12 by Moutig
 *
 * This header defines the ELF64 structures and constants needed to parse
 * ELF files, as well as a handle for loaded ELF files that provides
 * access to their headers and sections.
 */

#ifndef HAJ_ELF_H
# define HAJ_ELF_H

# include <stddef.h>
# include <stdint.h>

# ifdef __cplusplus
extern "C" {
# endif

/* ----- ELF64 types ----- */

typedef uint64_t	Elf64_Addr;
typedef uint64_t	Elf64_Off;
typedef uint16_t	Elf64_Half;
typedef uint32_t	Elf64_Word;
typedef int32_t		Elf64_Sword;
typedef uint64_t	Elf64_Xword;
typedef int64_t		Elf64_Sxword;

/**
 * @brief ELF64 header structure.
 *
 * This structure represents the header of an ELF64 file, containing
 * information about the file type, architecture, entry point, and
 * offsets to program and section headers.
 */
typedef struct {
	unsigned char	e_ident[16];
	Elf64_Half		e_type;
	Elf64_Half		e_machine;
	Elf64_Word		e_version;
	Elf64_Addr		e_entry;
	Elf64_Off		e_phoff;
	Elf64_Off		e_shoff;
	Elf64_Word		e_flags;
	Elf64_Half		e_ehsize;
	Elf64_Half		e_phentsize;
	Elf64_Half		e_phnum;
	Elf64_Half		e_shentsize;
	Elf64_Half		e_shnum;
	Elf64_Half		e_shstrndx;
} Elf64_Ehdr;

/**
 * @brief ELF64 program header structure.
 *
 * This structure represents a program header in an ELF64 file, describing
 * a segment to be loaded into memory, including its type, offset, virtual
 * address, physical address, file size, memory size, flags, and alignment.
 */
typedef struct {
	Elf64_Word	sh_name;
	Elf64_Word	sh_type;
	Elf64_Xword	sh_flags;
	Elf64_Addr	sh_addr;
	Elf64_Off	sh_offset;
	Elf64_Xword	sh_size;
	Elf64_Word	sh_link;
	Elf64_Word	sh_info;
	Elf64_Xword	sh_addralign;
	Elf64_Xword	sh_entsize;
} Elf64_Shdr;

/**
 * @brief ELF64 symbol table entry structure.
 *
 * This structure represents an entry in the symbol table of an ELF64 file,
 * containing information about a symbol's name, type, visibility, section
 * index, value, and size.
 */
typedef struct {
	Elf64_Word		st_name;
	unsigned char	st_info;
	unsigned char	st_other;
	Elf64_Half		st_shndx;
	Elf64_Addr		st_value;
	Elf64_Xword		st_size;
} Elf64_Sym;

/**
 * @brief ELF64 relocation entry structure (without addend).
 *
 * This structure represents a relocation entry in an ELF64 file that does
 * not include an explicit addend, containing the offset, info, and other
 * fields needed for relocation processing.
 */
typedef struct {
	Elf64_Word	p_type;
	Elf64_Word	p_flags;
	Elf64_Off	p_offset;
	Elf64_Addr	p_vaddr;
	Elf64_Addr	p_paddr;
	Elf64_Xword	p_filesz;
	Elf64_Xword	p_memsz;
	Elf64_Xword	p_align;
} Elf64_Phdr;

/* ----- Constants ----- */

/* --- ELF file types --- */
# define ELF_ET_EXEC	2
# define ELF_ET_DYN		3
# define ELF_EM_X86_64	62
# define ELF_EM_AARCH64	183

/* --- ELF section types --- */
# define ELF_SHT_SYMTAB	2
# define ELF_SHT_STRTAB	3
# define ELF_SHT_NOBITS	8

/* --- ELF symbol types --- */
# define ELF_STT_NOTYPE		0
# define ELF_STT_OBJECT		1
# define ELF_STT_FUNC		2
# define ELF_STT_SECTION	3
# define ELF_STT_FILE		4
# define ELF_STT_COMMON		5
# define ELF_STT_TLS		6

/* --- ELF symbol bindings --- */
# define ELF_STB_LOCAL	0
# define ELF_STB_GLOBAL	1
# define ELF_STB_WEAK	2

# define ELF_SHN_UNDEF	0

/* ----- Program header types ----- */
# define PT_NULL	0
# define PT_LOAD	1
# define PT_DYNAMIC	2
# define PT_INTERP	3
# define PT_NOTE	4
# define PT_SHLIB	5
# define PT_PHDR	6
# define PT_TLS	7

/* ----- Program header flags ----- */
# define PF_X	0x1
# define PF_W	0x2
# define PF_R	0x4

# define ELF_ST_TYPE(i)	((i) & 0xF)
# define ELF_ST_BIND(i)	((i) >> 4)

# define ElfW(type) Elf64_##type

/* ----- Loaded ELF handle ----- */

/**
 * @brief Opaque handle for a loaded ELF file.
 *
 * This structure represents a loaded ELF file, providing access to its
 * headers and sections. The actual definition is hidden from users of
 * this API.
 */
typedef struct s_elf s_elf;

/**
 * @brief Open an ELF file.
 *
 * This function opens an ELF file and returns a handle to it.
 *
 * @param path The path to the ELF file.
 * @return A handle to the opened ELF file, or NULL if an error occurred.
 */
s_elf	*elf_open(const char *path);

/**
 * @brief Close an ELF file.
 *
 * This function closes a previously opened ELF file and releases any
 * associated resources.
 *
 * @param e The handle to the ELF file to close.
 */
s_elf	*elf_open_self(void);

/**
 * @brief Open the current process's ELF file.
 *
 * This function opens the ELF file corresponding to the current process
 * and returns a handle to it.
 *
 * @return A handle to the current process's ELF file, or NULL if an error occurred.
 */
void	elf_close(s_elf *e);


/**
 * @brief Get the base address of the loaded ELF file.
 *
 * This function returns the base address at which the ELF file is loaded
 * in memory.
 *
 * @param e The handle to the ELF file.
 * @return The base address of the loaded ELF file.
 */
const void			*elf_base(const s_elf *e);

/**
 * @brief Get the size of the loaded ELF file.
 *
 * This function returns the size of the loaded ELF file in bytes.
 *
 * @param e The handle to the ELF file.
 * @return The size of the loaded ELF file in bytes.
 */
size_t				elf_size(const s_elf *e);

/**
 * @brief Get the ELF64 header of the loaded ELF file.
 *
 * This function returns a pointer to the ELF64 header of the loaded
 * ELF file.
 *
 * @param e The handle to the ELF file.
 * @return A pointer to the ELF64 header of the loaded ELF file.
 */
const Elf64_Ehdr	*elf_ehdr(const s_elf *e);

/**
 * @brief Get the ELF64 section header at the specified index.
 *
 * This function returns a pointer to the ELF64 section header at the
 * specified index in the loaded ELF file.
 *
 * @param e The handle to the ELF file.
 * @param idx The index of the section header to retrieve.
 * @return A pointer to the ELF64 section header at the specified index.
 */
const Elf64_Shdr	*elf_shdr(const s_elf *e, unsigned idx);

/**
 * @brief Get the number of section headers in the loaded ELF file.
 *
 * This function returns the number of section headers present in the
 * loaded ELF file.
 *
 * @param e The handle to the ELF file.
 * @return The number of section headers in the loaded ELF file.
 */
unsigned			elf_shnum(const s_elf *e);



/**
 * @brief Get the section header string table of the loaded ELF file.
 *
 * This function returns a pointer to the section header string table
 * of the loaded ELF file, which contains the names of the sections.
 *
 * @param e The handle to the ELF file.
 * @return A pointer to the section header string table of the loaded ELF file.
 */
const char			*elf_shstrtab(const s_elf *e);

/**
 * @brief Get the size of the section header string table.
 *
 * This function returns the size of the section header string table
 * in bytes.
 *
 * @param e The handle to the ELF file.
 * @return The size of the section header string table in bytes.
 */
size_t				elf_shstrtab_size(const s_elf *e);


/**
 * @brief Get the name of the section at the specified index.
 *
 * This function returns the name of the section at the specified index
 * in the loaded ELF file.
 *
 * @param e The handle to the ELF file.
 * @param idx The index of the section.
 * @return The name of the section at the specified index, or NULL if an error occurred.
 */
const char			*elf_section_name(const s_elf *e, unsigned idx);

/**
 * @brief Find a section by name.
 *
 * This function searches for a section with the specified name in the
 * loaded ELF file and returns its index.
 *
 * @param e The handle to the ELF file.
 * @param name The name of the section to find.
 * @return The index of the section with the specified name, or -1 if not found.
 */
int					elf_find_section(const s_elf *e, const char *name);

/**
 * @brief Find a section by type.
 *
 * This function searches for a section with the specified type in the
 * loaded ELF file and returns its index.
 *
 * @param e The handle to the ELF file.
 * @param type The type of the section to find.
 * @return The index of the section with the specified type, or -1 if not found.
 */
int					elf_find_section_by_type(const s_elf *e, uint32_t type);

/**
 * @brief Get the data of a section at the specified index.
 *
 * This function returns a pointer to the data of the section at the
 * specified index in the loaded ELF file, along with its size.
 *
 * @param e The handle to the ELF file.
 * @param idx The index of the section.
 * @param out_size Pointer to a variable to receive the size of the section data.
 * @return A pointer to the section data, or NULL if an error occurred.
 */
const void			*elf_section_data(const s_elf *e, unsigned idx, size_t *out_size);

typedef int (*elf_symbol_cb)(const char	*name,
							 uintptr_t	value,
							 size_t		size,
							 uint8_t	type,
							 uint8_t	bind,
							 uint16_t	shndx,
							 void		*arg);

/**
 * @brief Iterate over all symbols in the loaded ELF file.
 *
 * This function iterates over all symbols in the loaded ELF file,
 * invoking the specified callback for each symbol. The iteration stops
 * if the callback returns a non-zero value.
 *
 * @param e The handle to the ELF file.
 * @param cb The callback function to invoke for each symbol.
 * @param arg User-defined argument to pass to the callback function.
 * @return 0 on success, -1 on error.
 */
int elf_foreach_symbol(const s_elf *e, elf_symbol_cb cb, void *arg);

# ifdef __cplusplus
}
# endif

#endif
