/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file dwarf_line.c
 * @brief DWARF line number program parsing and callback handling.
 * @Created: 2026/10/28 09:11:10 by Moutig
 * @Updated: 2026/10/29 06:54:38 by Moutig
 *
 * Emits one callback per row. Storage of the rows is entirely
 * delegated to the caller; nothing is allocated here except the
 * per-CU file/dir tables, via di_arena_alloc(d, ...).
 */

#include "./internal.h"
#include "../../include/stddef.h"

/* DW_FORM_strp is resolved to "" (no .debug_str loaded). */
/**
 * @brief Global string table for DWARF line number information.
 * This is used to resolve DW_FORM_strp form values.
 */
static const char	*g_lineStr;
/**
 * @brief Size of the global string table for DWARF line number information.
 * This is used to determine the bounds of the string table.
 */
static size_t		g_lineStrSize;

/**
 * @brief Read a DWARF form value.
 * @param pp Pointer to the current position in the input data.
 * @param end Pointer to the end of the input data.
 * @param form The DWARF form to read.
 * @param addrS9ze The size of an address in bytes.
 * @param uval Pointer to the output unsigned integer value.
 * @param sval Pointer to the output string value.
 * @return (0) on success, -1 on failure.
 */
static int readForm(const uint8_t **pp, const uint8_t *end, uint16_t form, unsigned addrS9ze, uint64_t *uval, const char **sval)
{
	uint64_t v = 0;
	*uval = 0; *sval = NULL;
	switch (form) {
	case 0x01:	/* DW_FORM_addr */
		if (di_rd(pp, end, addrS9ze, &v) < 0) return (-1);
		*uval = v; return (0);
	case 0x08:	/* DW_FORM_strp */
	{
		const uint8_t *s = *pp;
		while (*pp < end && **pp) (*pp)++;
		if (*pp >= end) return (-1);
		(*pp)++;
		*sval = (const char *)s;
		return (0);
	}
	case 0x0e:	/* DW_FORM_string */
		if (di_rd(pp, end, 4, &v) < 0) return (-1);
		*sval = ""; return (0);
	case 0x1f:	/* DW_FORM_strp_sup */
		if (di_rd(pp, end, 4, &v) < 0) return (-1);
		*sval = (g_lineStr && v < g_lineStrSize) ? g_lineStr + v : "";
		return (0);
	case 0x0b:	/* DW_FORM_data1 */
		if (di_rd(pp, end, 1, &v) < 0) return (-1);
		*uval = v; return (0);
	case 0x05:	/* DW_FORM_data2 */
		if (di_rd(pp, end, 2, &v) < 0) return (-1);
		*uval = v; return (0);
	case 0x06:	/* DW_FORM_data4 */
		if (di_rd(pp, end, 4, &v) < 0) return (-1);
		*uval = v; return (0);
	case 0x07:	/* DW_FORM_data8 */
		if (di_rd(pp, end, 8, &v) < 0) return (-1);
		*uval = v; return (0);
	case 0x0f:	/* DW_FORM_uleb128 */
		*uval = di_uleb(pp, end); return (0);
	case 0x1e:	/* DW_FORM_block1 */
		if ((size_t)(end - *pp) < 16)
			return (-1);
		*pp += 16;
		return (0);
	case 0x09:	/* DW_FORM_block2 */
	{
		uint64_t len = di_uleb(pp, end);
		if ((size_t)(end - *pp) < len) return (-1);
		*pp += len; return (0);
	}
	default: return (-1);
	}
}

static const char *fname(const char **ft, unsigned nfiles, unsigned file)
{
	if (!ft || file > nfiles) return (NULL);
	return (ft[file]);
}

/**
 * @brief Read the DWARF line number program file table (version 4).
 *
 * This function reads the file table from the DWARF line number program
 * for version 4. It populates the output file table and the number of files.
 *
 * @param d The debug information object.
 * @param pp Pointer to the current position in the input data.
 * @param end Pointer to the end of the input data.
 * @param out Pointer to the output file table.
 * @param nfilesOut Pointer to the output number of files.
 * @return (0) on success, -1 on failure.
 */
static int v4Files(di_object *d, const uint8_t **pp, const uint8_t *end, const char ***out, unsigned *nfilesOut)
{
	const char		**dirs, **tmp;
	const uint8_t	*scan;
	unsigned		ndirs = 0, count = 0;

	/* First pass: count directories (NUL-terminated strings, "" ends) */
	scan = *pp;
	while (scan < end && *scan != '\0' && ndirs < 65535) {
		while (scan < end && *scan != '\0') scan++;
		if (scan < end) scan++;
		ndirs++;
	}

	/* Allocate one-indexed dir table (v4 is 1-based) */
	dirs = di_arena_alloc(d, (ndirs + 1) * sizeof(char *));
	if (!dirs) return (-1);
	dirs[0] = NULL;

	/* Second pass: read directory strings */
	for (unsigned k = 1; k <= ndirs; k++) {
		dirs[k] = (const char *)*pp;
		while (*pp < end && **pp != '\0') (*pp)++;
		if (*pp < end) (*pp)++;
	}
	if (*pp < end) (*pp)++;	/* skip terminating empty string */

	/* Count file entries (name + 3 ulebs each, "" ends) */
	scan = *pp;
	while (scan < end && *scan != '\0' && count < 65535) {
		while (scan < end && *scan != '\0') scan++;
		if (scan < end) scan++;
		(void)di_uleb(&scan, end);
		(void)di_uleb(&scan, end);
		(void)di_uleb(&scan, end);
		count++;
	}

	/* Allocate one-indexed file table (index 0 unused) */
	tmp = di_arena_alloc(d, (count + 1) * sizeof(char *));
	if (!tmp) return (-1);
	tmp[0] = NULL;

	/* Read file entries: name + dirIdx (+ mtime, size ignored) */
	for (unsigned k = 1; k <= count; k++) {
		const char *name = (const char *)*pp;
		uint64_t didx;
		while (*pp < end && **pp != '\0') (*pp)++;
		if (*pp < end) (*pp)++;
		didx = di_uleb(pp, end);
		(void)di_uleb(pp, end);
		(void)di_uleb(pp, end);
		tmp[k] = di_path_join(d, (didx > 0 && didx <= ndirs) ? dirs[didx] : NULL, name);
	}
	*out = tmp;
	*nfilesOut = count;
	return (0);
}

/**
 * @brief Read the DWARF line number program file table (version 5).
 *
 * This function reads the file table from the DWARF line number program
 * for version 5. It populates the output file table and the number of files.
 *
 * @param d The debug information object.
 * @param pp Pointer to the current position in the input data.
 * @param end Pointer to the end of the input data.
 * @param as The address size in bytes.
 * @param out Pointer to the output file table.
 * @param nfilesOut Pointer to the output number of files.
 * @return (0) on success, -1 on failure.
 */
static int v5Files(di_object *d, const uint8_t **pp, const uint8_t *end, unsigned as, const char ***out, unsigned *nfilesOut)
{
	uint16_t	dFC[16], dFF[16], fFC[16], fFF[16];
	uint64_t	nDF, nFF, nDirs, nFiles;
	const char	**dirs = NULL, **ft;

	if (as > 8 || *pp >= end) return (-1);	/* Validate address size and cursor */

	/* Directory entry format: count + (content, form) pairs */
	nDF = *(*pp)++;
	if (nDF > 16) return (-1);
	for (uint64_t i = 0; i < nDF; i++) {
		dFC[i] = (uint16_t)di_uleb(pp, end);
		dFF[i] = (uint16_t)di_uleb(pp, end);
	}

	/* Directory table: count, then one entry per directory */
	nDirs = di_uleb(pp, end);
	if (nDirs > 65536) return (-1);
	if (nDirs > 0) {
		dirs = di_arena_alloc(d, nDirs * sizeof(char *));
		if (!dirs) return (-1);
	}
	for (uint64_t i = 0; i < nDirs; i++) {
		const char *path = "";
		for (uint64_t f = 0; f < nDF; f++) {
			uint64_t u = 0; const char *s = NULL;
			if (readForm(pp, end, dFF[f], as, &u, &s) < 0) return (-1);
			if (dFC[f] == 0x1 && s) path = s;
		}
		dirs[i] = path;
	}

	/* File-name entry format: count + (content, form) pairs */
	if (*pp >= end) return (-1);
	nFF = *(*pp)++;
	if (nFF > 16) return (-1);
	for (uint64_t i = 0; i < nFF; i++) {
		fFC[i] = (uint16_t)di_uleb(pp, end);
		fFF[i] = (uint16_t)di_uleb(pp, end);
	}

	/* File-name table: count, then one entry per file */
	nFiles = di_uleb(pp, end);
	if (nFiles > 65536) return (-1);
	if (nFiles == 0) { *out = NULL; *nfilesOut = 0; return (0); }

	/* Allocate zero-indexed output table (v5 `file` is 0-based) */
	ft = di_arena_alloc(d, nFiles * sizeof(char *));
	if (!ft) return (-1);

	/* Decode each file entry: path + dirIdx, resolve relative paths */
	for (uint64_t i = 0; i < nFiles; i++) {
		const char *path = ""; uint64_t dirIdx = 0; const char *dir = NULL;
		for (uint64_t f = 0; f < nFF; f++) {
			uint64_t u = 0; const char *s = NULL;
			if (readForm(pp, end, fFF[f], as, &u, &s) < 0) return (-1);
			if (fFC[f] == 0x1 && s) path = s;
			else if (fFC[f] == 0x2) dirIdx = u;
		}
		/* Relative dir: prepend compilation directory (dirs[0]). */
		if (dirs && dirIdx < nDirs) {
			dir = dirs[dirIdx];
			if (dir && dir[0] != '/' && dirIdx > 0 && dirs[0])
				dir = di_path_join(d, dirs[0], dir);
		}
		ft[i] = di_path_join(d, dir, path);
	}
	*out = ft;
	*nfilesOut = (unsigned)(nFiles - 1);
	return (0);
}

/**
 * @brief Run the DWARF line number program.
 *
 * This function executes the DWARF line number program, invoking the provided
 * callback for each row generated by the program. It handles both standard and
 * special opcodes, as well as extended opcodes.
 *
 * @param d The debug information object.
 * @param p Pointer to the current position in the input data.
 * @param end Pointer to the end of the input data.
 * @param minInst Minimum instruction length.
 * @param maxOps Maximum number of operations per instruction.
 * @param lineBase Line base value.
 * @param lineRange Line range value.
 * @param opcodeBase Opcode base value.
 * @param stdLens Standard opcode lengths array.
 * @param ft File table array.
 * @param nfiles Number of files in the file table.
 * @param cb Callback function to invoke for each row.
 * @param arg User-defined argument to pass to the callback function.
 * @return (0) on success, -1 on failure.
 */
static int runProgram(di_object	*d,	const	uint8_t			*p,	const uint8_t *end,
					   unsigned		minInst,	unsigned		maxOps,
					   int			lineBase,	unsigned		lineRange,
					   unsigned		opcodeBase,	const uint8_t	*stdLens,
					   const char	**ft,		unsigned		nfiles,
					   di_line_cb	cb,			void			*arg)
{
	uintptr_t	addr = 0;
	unsigned	opIdx = 0, file = 1;
	int64_t		line = 1;

	(void)d;
	while (p < end) {
		uint8_t op = *p++;
		if (op >= opcodeBase) {	/* special opcode */
			unsigned adj = op - opcodeBase, pc_adv;
			if (maxOps <= 1) pc_adv = (adj / lineRange) * minInst;
			else {
				unsigned total = opIdx + adj / lineRange;
				pc_adv = (total / maxOps) * minInst;
				opIdx = total % maxOps;
			}
			addr += pc_adv;
			line += lineBase + (int)(adj % lineRange);
			cb(addr, fname(ft, nfiles, file), (uint32_t)line, 0, arg);
		} else if (op == 0) {	/* extended opcode */
			uint64_t len = di_uleb(&p, end);
			const uint8_t *next;
			uint8_t sub;
			if (len == 0) continue;
			if ((uint64_t)(end - p) < len) return (0);
			next = p + len;
			sub = *p++;
			if (sub == 1) {
				cb(addr, fname(ft, nfiles, file), (uint32_t)line, 1, arg);
				addr = 0; opIdx = 0; file = 1; line = 1;
			} else if (sub == 2) {
				uint64_t v = 0;
				unsigned nn = (unsigned)(len - 1);
				if (nn > 8) nn = 8;
				if (di_rd(&p, next, nn, &v) == 0) addr = (uintptr_t)v;
				opIdx = 0;
			}
			p = next;
		} else {	/* standard opcode */
			switch (op) {
			case 1:
				cb(addr, fname(ft, nfiles, file), (uint32_t)line, 0, arg);
				break;
			case 2: addr += (uintptr_t)(di_uleb(&p, end) * minInst); break;
			case 3: line += di_sleb(&p, end); break;
			case 4: file = (unsigned)di_uleb(&p, end); break;
			case 5: (void)di_uleb(&p, end); break;
			case 6: case 7: break;
			case 8:
				addr += ((255u - opcodeBase) / lineRange) * minInst;
				break;
			case 9: {
				uint64_t adv = 0;
				if (di_rd(&p, end, 2, &adv) == 0) addr += adv;
				opIdx = 0;
				break;
			}
			case 10: case 11: break;
			case 12: (void)di_uleb(&p, end); break;
			default:
				for (unsigned i = 0; i < stdLens[op - 1]; i++)
					(void)di_uleb(&p, end);
				break;
			}
		}
	}
	return (0);
}

/**
 * @brief Iterate over source information in a debug information object.
 *
 * This function iterates over the source information in the provided debug
 * information object, invoking the specified callback for each source entry.
 *
 * @param d The debug information object.
 * @param cb Callback function to invoke for each source entry.
 * @param arg User-defined argument to pass to the callback function.
 * @return (0) on success, -1 on failure, or the return value of the callback if non-zero.
 */
static int parseCu(di_object *d, const uint8_t *p, const uint8_t *cuEnd, unsigned version, di_line_cb cb, void *arg)
{
	uint64_t		headerLen = 0;
	unsigned		addrS9ze = 8, minInst, maxOps = 1;
	int				lineBase;
	unsigned		lineRange, opcodeBase;
	const uint8_t	*prog, *stdLens;
	const char		**files = NULL;
	unsigned		nfiles = 0;

	if (version >= 5) {
		if (cuEnd - p < 2) return (-1);
		addrS9ze = *p++;
		p++;
	}
	if (di_rd(&p, cuEnd, 4, &headerLen) < 0) return (-1);
	if ((uint64_t)(cuEnd - p) < headerLen) return (-1);
	prog = p + headerLen;
	if (prog - p < (version >= 4 ? 6 : 5)) return (-1);
	minInst = *p++;
	if (version >= 4) maxOps = *p++;
	p++;
	lineBase = (int8_t)*p++;
	lineRange = *p++;
	opcodeBase = *p++;
	if (lineRange == 0 || opcodeBase == 0 || opcodeBase > 32) return (-1);
	if ((size_t)(prog - p) < (size_t)(opcodeBase - 1)) return (-1);
	stdLens = p;
	p += opcodeBase - 1;

	if (version >= 5) {
		if (v5Files(d, &p, prog, addrS9ze, &files, &nfiles) < 0) return (-1);
	} else {
		if (v4Files(d, &p, prog, &files, &nfiles) < 0) return (-1);
	}
	return (runProgram(d, prog, cuEnd, minInst, maxOps,
					  lineBase, lineRange, opcodeBase,
					  stdLens, files, nfiles, cb, arg));
}

int di_dwarf_parse_line(di_object *d, di_line_cb cb, void *arg)
{
	const s_elf		*e = d->elf;
	int				di = elf_find_section(e, ".debug_line");
	int				dsi;
	size_t			dlineSize = 0;
	const uint8_t	*base, *end, *p;

	g_lineStr = NULL;
	g_lineStrSize = 0;
	if (di < 0) return (0);
	base = elf_section_data(e, (unsigned)di, &dlineSize);
	if (!base) return (0);

	dsi = elf_find_section(e, ".debug_line_str");
	if (dsi >= 0) {
		size_t sz = 0;
		g_lineStr = elf_section_data(e, (unsigned)dsi, &sz);
		g_lineStrSize = sz;
	}

	end	= base + dlineSize;
	p	= base;
	while (end - p >= 4) {
		uint64_t unitLen = 0, ver64 = 0;
		const uint8_t *cuEnd;
		(void)di_rd(&p, end, 4, &unitLen);
		if (unitLen == 0xffffffffULL) {
			if (di_rd(&p, end, 8, &unitLen) < 0) break;
			if ((uint64_t)(end - p) < unitLen) break;
			p += unitLen;
			continue;
		}
		if (unitLen == 0) continue;
		if ((uint64_t)(end - p) < unitLen) break;
		cuEnd = p + unitLen;
		if (di_rd(&p, cuEnd, 2, &ver64) == 0 && ver64 >= 2 && ver64 <= 5)
			(void)parseCu(d, p, cuEnd, (unsigned)ver64, cb, arg);
		p = cuEnd;
	}
	return (0);
}
