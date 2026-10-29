/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file lookup.c
 * @brief Debug information lookup functions for hajlibc.
 * @Created: 2026/10/28 09:11:32 by Moutig
 * @Updated: 2026/10/29 06:51:12 by Moutig
 *
 * This file implements functions for looking up debug information,
 * such as function names and source file locations,
 * based on addresses in the program's memory.
 * It provides an interface for retrieving this information from ELF files and DWARF debugging data.
 */

#include "./internal.h"
#include "../../include/link.h"

#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

typedef struct {
	di_object	*d;
	size_t		idx;
} build_ctx;


/**
 * @brief Copy a string with a maximum length.
 * @param dst The destination buffer.
 * @param src The source string.
 * @param cap The maximum number of characters to copy (including the null terminator).
 * @return The number of characters copied.
 */
static void diStrlcpy(char *dst, const char *src, size_t cap)
{
	size_t i = 0;

	if (dst == NULL || cap == 0)
		return;
	if (src != NULL) {
		while (src[i] != '\0' && i + 1 < cap) {
			dst[i] = src[i];
			i++;
		}
	}
	dst[i] = '\0';
}

static size_t diObjSize(void)
{
	return ((sizeof(di_object) + 4095u) & ~(size_t)4095u);
}

static di_object *diObjAlloc(void)
{
	void *p = mmap(NULL, diObjSize(), PROT_READ | PROT_WRITE,
				   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (p == MAP_FAILED)
		return (NULL);
	memset(p, 0, sizeof(di_object));
	return (p);
}

static void diObjFreeStruct(di_object *d)
{
	if (d != NULL)
		munmap(d, diObjSize());
}

/**
 * @brief Count the number of function symbols in the ELF file.
 *
 * This function is used as a callback for iterating over the symbols in the ELF file.
 * It increments the count for each function symbol encountered.
 *
 * @param name The name of the symbol.
 * @param value The value (address) of the symbol.
 * @param size The size of the symbol.
 * @param type The type of the symbol (e.g., function, object).
 * @param bind The binding of the symbol (e.g., local, global).
 * @param shndx The section index of the symbol.
 * @param arg A pointer to a size_t variable that holds the count of function symbols.
 * @return (0) to continue iteration, or a non-zero value to stop.
 */
static int countFuncCb(const char	*name,	uintptr_t	value,	size_t		size,
						 uint8_t	type,	uint8_t		bind,	uint16_t	shndx,
						 void		*arg)
{
	(void)name; (void)value; (void)size; (void)bind;
	(void)type; (void)shndx;
	(*(size_t *)arg)++;
	return (0);
}

/**
 * @brief Fill the function symbols in the debug object.
 *
 * This function is used as a callback for iterating over the symbols in the ELF file.
 * It populates the function symbols in the debug object with their addresses and names.
 *
 * @param name The name of the symbol.
 * @param value The value (address) of the symbol.
 * @param size The size of the symbol.
 * @param type The type of the symbol (e.g., function, object).
 * @param bind The binding of the symbol (e.g., local, global).
 * @param shndx The section index of the symbol.
 * @param arg A pointer to a build_ctx structure that holds the debug object and index.
 * @return (0) to continue iteration, or a non-zero value to stop.
 */
static int fillFuncCb(const char	*name,	uintptr_t	value,	size_t		size,
						uint8_t		type,	uint8_t		bind,	uint16_t	shndx,
						void		*arg)
{
	build_ctx *ctx = arg;

	(void)bind;
	if (type != ELF_STT_FUNC) return (0);
	if (shndx == ELF_SHN_UNDEF || value == 0) return (0);
	if (!name || name[0] == '\0') return (0);

	ctx->d->funcs[ctx->idx].lo		= value;
	ctx->d->funcs[ctx->idx].hi		= value + size;
	ctx->d->funcs[ctx->idx].name	= name;
	ctx->idx++;
	return (0);
}

static int cmpFunc(const void *a, const void *b)
{
	const di_func *x = a, *y = b;
	if (x->lo != y->lo) return (x->lo < y->lo) ? -1 : 1;
	return (0);
}

static int countLineCb(uintptr_t addr, const char *file, uint32_t line, int endSeq, void *arg)
{
	(void)addr; (void)file; (void)line; (void)endSeq;
	(*(size_t *)arg)++;
	return (0);
}

static int fillLineCb(uintptr_t addr, const char *file, uint32_t line, int endSeq, void *arg)
{
	build_ctx *ctx = arg;
	di_object *d = ctx->d;

	d->lines[ctx->idx].addr			= addr;
	d->lines[ctx->idx].file			= file;
	d->lines[ctx->idx].line			= line;
	d->lines[ctx->idx].seq			= (uint32_t)ctx->idx;
	d->lines[ctx->idx].endSequence	= (uint8_t)endSeq;
	ctx->idx++;
	return (0);
}

static int cmpLine(const void *a, const void *b)
{
	const di_lne *x = a, *y = b;
	if (x->addr != y->addr) return ((x->addr < y->addr) ? -1 : 1);
	if (x->endSequence != y->endSequence)
		return ((x->endSequence > y->endSequence) ? -1 : 1);
	return (x->seq < y->seq) ? -1 : ((x->seq > y->seq) ? 1 : 0);
}

/**
 * @brief Build indexes for function symbols and source lines in the debug object.
 *
 * This function counts the number of function symbols and source lines in the ELF file,
 * allocates memory for them, and fills the debug object with the relevant information.
 *
 * @param d The debug object to populate with function symbols and source lines.
 */
static void buildIndexes(di_object *d)
{
	s_elf*e = d->elf;
	size_t n = 0;

	/* ----- functions ----- */
	elf_foreach_symbol(e, countFuncCb, &n);
	if (n > 0) {
		d->funcs = di_arena_alloc(d, n * sizeof(di_func));
		if (d->funcs) {
			build_ctx ctx = { d, 0 };
			elf_foreach_symbol(e, fillFuncCb, &ctx);
			d->nfuncs = ctx.idx;
			qsort(d->funcs, d->nfuncs, sizeof(di_func), cmpFunc);
			for (size_t i = 0; i < d->nfuncs; i++) {
				if (d->funcs[i].hi > d->funcs[i].lo) continue;
				size_t j = i + 1;
				while (j < d->nfuncs
					   && d->funcs[j].lo == d->funcs[i].lo)
					j++;
				d->funcs[i].hi = (j < d->nfuncs)
					? d->funcs[j].lo : d->funcs[i].lo + 1;
			}
		}
	}

	/* ----- lines ----- */
	n = 0;
	di_dwarf_parse_line(d, countLineCb, &n);
	if (n > 0) {
		d->lines = di_arena_alloc(d, n * sizeof(di_lne));
		if (d->lines) {
			build_ctx ctx = { d, 0 };
			di_dwarf_parse_line(d, fillLineCb, &ctx);
			d->nlines = ctx.idx;
			qsort(d->lines, d->nlines, sizeof(di_lne), cmpLine);
		}
	}
}

static di_object	*g_self;
static int			g_ready;
static char			g_lastOpenError[256];

di_object *di_object_open_self(void)
{
	return (di_object_open("/proc/self/exe"));
}

di_object *di_object_open(const char *path)
{
	di_object	*d;
	s_elf		*e;

	e = elf_open(path);
	if (!e) return (NULL);

	d = diObjAlloc();
	if (!d) { elf_close(e); return (NULL); }

	d->elf = e;
	if (path)
	{
		diStrlcpy(d->pathBuff, path, sizeof(d->pathBuff));

		if (elf_ehdr(e)->e_type != ELF_ET_EXEC) {
			diStrlcpy(g_lastOpenError,
					  "PIE / ET_DYN binary (load base unknown), link with -no-pie",
					  sizeof(g_lastOpenError));
			di_object_close(d);
			return (NULL);
		}
	}

	buildIndexes(d);

	if (d->nfuncs == 0 && d->nlines == 0) {
		diStrlcpy(g_lastOpenError,
				  "no function symbols and no line info",
				  sizeof(g_lastOpenError));
		di_object_close(d);
		return (NULL);
	}
	if (d->nfuncs == 0)
		diStrlcpy(d->warn, "no function names", sizeof(d->warn));
	else if (d->nlines == 0)
		diStrlcpy(d->warn, "no file:line", sizeof(d->warn));

	g_lastOpenError[0] = '\0';
	d->ready = 1;
	return (d);
}

void di_object_close(di_object *d)
{
	if (!d) return;
	di_arena_release(d);
	elf_close(d->elf);
	diObjFreeStruct(d);
}

const char *di_object_path(const di_object *d)
{
	return (d ? d->pathBuff : NULL);
}

size_t di_object_size(const di_object *d)
{
	return (d) ? elf_size(d->elf) : 0;
}

const char *di_object_error(const di_object *d)
{
	return ((d && d->err[0]) ? d->err : NULL);
}

const char *di_object_warning(const di_object *d)
{
	return ((d && d->warn[0]) ? d->warn : NULL);
}

static const di_func *findFunc(const di_object *d, uintptr_t addr)
{
	size_t			lo = 0, hi = d->nfuncs;
	const di_func	*f;

	if (d->nfuncs == 0) return (NULL);
	while (lo < hi) {
		size_t mid = (lo + hi) / 2;
		if (d->funcs[mid].lo <= addr) lo = mid + 1;
		else hi = mid;
	}
	if (lo == 0) return (NULL);
	f = &d->funcs[lo - 1];
	return ((addr < f->hi) ? f : NULL);
}

static const di_lne *findLine(const di_object *d, uintptr_t addr)
{
	size_t lo = 0, hi = d->nlines;
	const di_lne *l;

	if (d->nlines == 0) return (NULL);
	while (lo < hi) {
		size_t mid = (lo + hi) / 2;
		if (d->lines[mid].addr <= addr) lo = mid + 1;
		else hi = mid;
	}
	if (lo == 0) return (NULL);
	l = &d->lines[lo - 1];
	return (l->endSequence ? NULL : l);
}

int di_object_find_function(di_object *d, uintptr_t addr, di_symbol *out)
{
	const di_func *f;

	if (!d || !out) return (-1);
	memset(out, 0, sizeof(*out));
	f = findFunc(d, addr);
	if (!f) return (-1);
	out->name		= f->name;
	out->value		= f->lo;
	out->size		= f->hi - f->lo;
	out->type		= DI_SYM_FUNCTION;
	out->section	= 0;
	return (0);
}

int di_object_find_symbol(di_object *d, const char *name, di_symbol *out)
{
	if (!d || !out || !name) return (-1);
	memset(out, 0, sizeof(*out));
	for (size_t i = 0; i < d->nfuncs; i++) {
		if (strcmp(d->funcs[i].name, name) == 0) {
			out->name		= d->funcs[i].name;
			out->value		= d->funcs[i].lo;
			out->size		= d->funcs[i].hi - d->funcs[i].lo;
			out->type		= DI_SYM_FUNCTION;
			out->section		= 0;
			return (0);
		}
	}
	return (-1);
}

int di_object_find_source(di_object *d, uintptr_t addr, di_source *out)
{
	const di_lne *l;

	if (!d || !out) return (-1);
	memset(out, 0, sizeof(*out));
	l = findLine(d, addr);
	if (!l || !l->file || l->file[0] == '\0') return (-1);
	out->path = l->file;
	out->file = l->file;
	for (const char *p = l->file; *p; p++)
		if (*p == '/') out->file = p + 1;
	out->line = l->line;
	return (0);
}

int di_object_foreach_symbol(di_object *d, int (*cb)(const di_symbol *, void *), void *arg)
{
	if (!d || !cb) return (-1);
	for (size_t i = 0; i < d->nfuncs; i++) {
		di_symbol s;
		memset(&s, 0, sizeof(s));
		s.name		= d->funcs[i].name;
		s.value		= d->funcs[i].lo;
		s.size		= d->funcs[i].hi - d->funcs[i].lo;
		s.type		= DI_SYM_FUNCTION;
		s.section	= 0;
		int r = cb(&s, arg);
		if (r != 0) return (r);
	}
	return (0);
}

int di_object_foreach_source(di_object *d, int (*cb)(uintptr_t, const di_source *, void *), void *arg)
{
	if (!d || !cb) return (-1);
	for (size_t i = 0; i < d->nlines; i++) {
		const di_lne *l = &d->lines[i];
		di_source src;

		if (l->endSequence) continue;
		if (!l->file || l->file[0] == '\0') continue;
		memset(&src, 0, sizeof(src));
		src.path = l->file;
		src.file = l->file;
		for (const char *p = l->file; *p; p++)
			if (*p == '/') src.file = p + 1;
		src.line = l->line;
		int r = cb(l->addr, &src, arg);
		if (r != 0) return (r);
	}
	return (0);
}

int di_object_foreach_section(di_object *d, int (*cb)(const di_section *, void *), void *arg)
{
	const s_elf *e;

	if (!d || !cb) return (-1);
	e = d->elf;
	for (unsigned i = 0; i < elf_shnum(e); i++) {
		const Elf64_Shdr *sh = elf_shdr(e, i);
		di_section s;

		memset(&s, 0, sizeof(s));
		s.name		= elf_section_name(e, i);
		s.type		= sh->sh_type;
		s.addr		= sh->sh_addr;
		s.offset	= sh->sh_offset;
		s.size		= sh->sh_size;
		int r = cb(&s, arg);
		if (r != 0) return (r);
	}
	return (0);
}

const void *di_object_section_data(di_object *d, const char *name, size_t *outSize)
{
	int idx;

	if (!d || !name) return (NULL);
	idx = elf_find_section(d->elf, name);
	if (idx < 0) return (NULL);
	return elf_section_data(d->elf, (unsigned)idx, outSize);
}

typedef struct {
	di_loaded_cb	cb;
	void			*arg;
} di_loaded_ctx;

static int loaded_cb(struct dl_phdr_info *info, size_t sz, void *arg)
{
	di_loaded_ctx	*ctx = arg;
	uintptr_t		lo = 0, hi = 0;

	(void)sz;
	for (int i = 0; i < info->dlpi_phnum; i++) {
		const ElfW(Phdr) *ph = &info->dlpi_phdr[i];
		uintptr_t a, b;

		if (ph->p_type != PT_LOAD) continue;
		a = info->dlpi_addr + ph->p_vaddr;
		b = a + ph->p_memsz;
		if (lo == 0 || a < lo) lo = a;
		if (b > hi) hi = b;
	}
	ctx->cb(info->dlpi_name ? info->dlpi_name : "",
			(uintptr_t)info->dlpi_addr, hi - lo, ctx->arg);
	return (0);
}

int di_enumerate_loaded(di_loaded_cb cb, void *arg)
{
	di_loaded_ctx	ctx;

	if (!cb) return (-1);
	ctx.cb = cb;
	ctx.arg = arg;
	dl_iterate_phdr(loaded_cb, &ctx);
	return (0);
}

int di_init(void)
{
	if (g_ready) return (0);
	g_self = di_object_open_self();
	if (!g_self) return (-1);
	g_ready = 1;
	return (0);
}

void di_fini(void)
{
	if (!g_self) return;
	di_object_close(g_self);
	g_self = NULL;
	g_ready = 0;
}

int di_lookup(uintptr_t addr, di_info *out)
{
	di_symbol	fn;
	di_source	src;
	int			have_fn = 0, have_src = 0;

	if (!g_ready || !out) return (-1);
	memset(out, 0, sizeof(*out));

	if (di_object_find_function(g_self, addr, &fn) == 0) {
		out->func		= fn.name;
		out->func_lo	= fn.value;
		have_fn			= 1;
	}
	if (di_object_find_source(g_self, addr, &src) == 0) {
		out->path	= src.path;
		out->file	= src.file;
		out->line	= src.line;
		have_src	= 1;
	}
	return ((have_fn || have_src) ? 0 : -1);
}

const char *di_error(void)
{
	if (g_self) {
		const char *err = di_object_error(g_self);
		if (err) return (err);
		if (g_lastOpenError[0]) return (g_lastOpenError);
		return (NULL);
	}
	if (g_lastOpenError[0]) return (g_lastOpenError);
	return ("di_init() not called");
}

const char *di_warning(void)
{
	return (g_self ? di_object_warning(g_self) : NULL);
}

void di_counts(size_t *nfuncs, size_t *nlines)
{
	if (nfuncs) *nfuncs = g_self ? g_self->nfuncs : 0;
	if (nlines) *nlines = g_self ? g_self->nlines : 0;
}
