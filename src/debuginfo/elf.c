/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file elf.c
 * @brief Functions for loading and parsing ELF files, and retrieving section information.
 * @Created: 2026/10/28 08:35:32 by Moutig
 * @Updated: 2026/10/29 05:18:19 by Moutig
 *
 * This file implements functions for loading ELF files into memory, parsing their headers,
 * and retrieving information about their sections. It provides an interface for working
 * with ELF files in the context of debug information lookup.
 */

#include "./internal.h"

#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

/**
 * @brief Internal structure representing an ELF file and its associated data.
 *
 * This structure holds information about the ELF file, including its base address,
 * size, section headers, and other relevant data.
 * It is used internally by the debuginfo module to manage and access ELF files.
 */
struct s_elf {
	void		*base;		/**< Base address of the mapped ELF file. */
	size_t		size;		/**< Size of the mapped ELF file. */
	size_t		mapSize;	/**< Size of the memory mapping for the ELF file. */

	Elf64_Ehdr	*ehdr;		/**< Pointer to the ELF header. */
	Elf64_Shdr	*shdrs;		/**< Pointer to the section headers. */
	unsigned	shnum;		/**< Number of section headers. */
	const char	*shstrtab;	/**< Pointer to the section header string table. */
	size_t		shstrtabSize;	/**< Size of the section header string table. */

	void		*selfMap;	/**< Pointer to the memory mapping of the current process's ELF file. */
	size_t		selfMapSize;	/**< Size of the memory mapping of the current process's ELF file. */
};

static __HAJ_INLINE size_t di_page_size(void)
{
	long ps = sysconf(_SC_PAGESIZE);
	return ((ps > 0) ? (size_t)ps : 4096u);
}

static __HAJ_INLINE size_t di_round_up(size_t v, size_t a)
{
	return ((v + a - 1) & ~(a - 1));
}

s_elf *elf_open_self(void)
{
	return (elf_open("/proc/self/exe"));
}

s_elf *elf_open(const char *path)
{
	s_elf	*e;
	int		fd;
	off_t	sz;
	size_t	pageSize;
	size_t	mapSize;
	size_t	got;
	void	*base;
	void	*selfMap;

	if (!path) return (NULL);

	fd = open(path, O_RDONLY);
	if (fd < 0) return (NULL);

	sz = lseek(fd, 0, SEEK_END);
	if (sz <= 0 || sz > (off_t)(512LL * 1024 * 1024)) {
		close(fd);
		return (NULL);
	}
	if (lseek(fd, 0, SEEK_SET) < 0) {
		close(fd);
		return (NULL);
	}

	pageSize	= di_page_size();
	mapSize		= di_round_up((size_t)sz, pageSize);

	selfMap = mmap(NULL, pageSize, PROT_READ | PROT_WRITE,
				   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (selfMap == MAP_FAILED) { close(fd); return (NULL); }

	e = (s_elf *)selfMap;
	memset(e, 0, sizeof(*e));
	e->selfMap		= selfMap;
	e->selfMapSize	= pageSize;

	base = mmap(NULL, mapSize, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (base == MAP_FAILED) {
		munmap(selfMap, pageSize);
		close(fd);
		return (NULL);
	}

	got = 0;
	while (got < (size_t)sz) {
		ssize_t r = read(fd, (uint8_t *)base + got, (size_t)sz - got);
		if (r <= 0) break;
		got += (size_t)r;
	}
	close(fd);

	if (got != (size_t)sz) {
		munmap(base, mapSize);
		munmap(selfMap, pageSize);
		return (NULL);
	}

	e->base		= base;
	e->size		= (size_t)sz;
	e->mapSize	= mapSize;

	if (e->size < sizeof(Elf64_Ehdr)) goto fail;
	e->ehdr = (Elf64_Ehdr *)e->base;

	if (memcmp(e->ehdr->e_ident, "\177ELF", 4) != 0
		|| e->ehdr->e_ident[4] != 2)	/* ELFCLASS64 */
		goto fail;
	if (e->ehdr->e_machine != ELF_EM_X86_64
		&& e->ehdr->e_machine != ELF_EM_AARCH64)
		goto fail;
	if (e->ehdr->e_shentsize != sizeof(Elf64_Shdr)
		|| e->ehdr->e_shoff == 0 || e->ehdr->e_shnum == 0)
		goto fail;
	if (e->ehdr->e_shoff + (uint64_t)e->ehdr->e_shnum * sizeof(Elf64_Shdr)
		> e->size)
		goto fail;

	e->shdrs = (Elf64_Shdr *)(void *)((uint8_t *)e->base + e->ehdr->e_shoff);
	e->shnum = e->ehdr->e_shnum;

	if (e->ehdr->e_shstrndx < e->shnum) {
		Elf64_Shdr *s = &e->shdrs[e->ehdr->e_shstrndx];
		if (s->sh_type == ELF_SHT_STRTAB
			&& s->sh_offset + s->sh_size <= e->size) {
			e->shstrtab		= (const char *)e->base + s->sh_offset;
			e->shstrtabSize	= (size_t)s->sh_size;
		}
	}
	return (e);

fail:
	munmap(base, mapSize);
	munmap(selfMap, pageSize);
	return (NULL);
}

void elf_close(s_elf *e)
{
	if (!e) return;
	if (e->base) munmap(e->base, e->mapSize);
	if (e->selfMap) munmap(e->selfMap, e->selfMapSize);
}

/* ----- Accessors ----- */

const void			*elf_base(const s_elf *e)	{ return (e->base); }
size_t				elf_size(const s_elf *e)	{ return (e->size); }
const Elf64_Ehdr	*elf_ehdr(const s_elf *e)	{ return (e->ehdr); }
const Elf64_Shdr	*elf_shdr(const s_elf *e, unsigned i)
{
	return ((i < e->shnum) ? &e->shdrs[i] : NULL);
}
unsigned	elf_shnum(const s_elf *e)			{ return e->shnum; }
const char	*elf_shstrtab(const s_elf *e)		{ return e->shstrtab; }
size_t		elf_shstrtab_size(const s_elf *e)	{ return e->shstrtabSize; }

const char *elf_section_name(const s_elf *e, unsigned i)
{
	if (!e->shstrtab || i >= e->shnum) return ("");
	if (e->shdrs[i].sh_name >= e->shstrtabSize) return ("");
	return (e->shstrtab + e->shdrs[i].sh_name);
}

int elf_find_section(const s_elf *e, const char *name)
{
	for (unsigned i = 0; i < e->shnum; i++)
		if (strcmp(elf_section_name(e, i), name) == 0)
			return ((int)i);
	return (-1);
}

int elf_find_section_by_type(const s_elf *e, uint32_t type)
{
	for (unsigned i = 0; i < e->shnum; i++)
		if (e->shdrs[i].sh_type == type)
			return ((int)i);
	return (-1);
}

const void *elf_section_data(const s_elf *e, unsigned i, size_t *out_size)
{
	const Elf64_Shdr *s;

	if (i >= e->shnum) return (NULL);
	s = &e->shdrs[i];
	if (s->sh_offset + s->sh_size > e->size) return (NULL);
	if (out_size) *out_size = (size_t)s->sh_size;
	if (s->sh_type == ELF_SHT_NOBITS) return (NULL);
	return ((const uint8_t *)e->base + s->sh_offset);
}

int elf_foreach_symbol(const s_elf *e, elf_symbol_cb cb, void *arg)
{
	int					si = elf_find_section(e, ".symtab");
	const Elf64_Sym		*syms;
	const char			*strs;
	size_t				n;
	const Elf64_Shdr	*symtab;
	const Elf64_Shdr	*strtab;

	if (si < 0) si = elf_find_section_by_type(e, ELF_SHT_SYMTAB);
	if (si < 0) return (0);

	symtab = &e->shdrs[si];
	if (symtab->sh_link >= e->shnum) return (0);
	strtab = &e->shdrs[symtab->sh_link];
	if (strtab->sh_type != ELF_SHT_STRTAB) return (0);
	if (symtab->sh_offset + symtab->sh_size > e->size
		|| strtab->sh_offset + strtab->sh_size > e->size)
		return (0);

	syms	= (const Elf64_Sym *)(void *)((const uint8_t *)e->base + symtab->sh_offset);
	strs	= (const char *)((const uint8_t *)e->base + strtab->sh_offset);
	n		= (size_t)(symtab->sh_size / sizeof(Elf64_Sym));

	for (size_t i = 0; i < n; i++) {
		const Elf64_Sym	*s = &syms[i];
		const char		*name = "";

		if (s->st_name < strtab->sh_size)
			name = strs + s->st_name;
		int r = cb(name,
				   (uintptr_t)s->st_value,
				   (size_t)s->st_size,
				   ELF_ST_TYPE(s->st_info),
				   ELF_ST_BIND(s->st_info),
				   s->st_shndx,
				   arg);
		if (r != 0) return (r);
	}
	return (0);
}
