/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file dl_iterate_phdr.c
 * @brief Implementation of dl_iterate_phdr for hajlibc.
 * @Created: 2026/10/27 08:26:51 by Moutig
 * @Updated: 2026/10/29 05:35:37 by Moutig
 *
 * This file provides an implementation of the dl_iterate_phdr function, which
 * iterates over the program headers of loaded ELF objects in the current process.
 */

#include "../../include/link.h"

#include <string.h>
#include <fcntl.h>
#include <unistd.h>

# define LK_MAPS_BUF_SIZE	(128 * 1024)
# define LK_MAX_PHDRS		32
# define LK_PATH_MAX		256

# define LK_ELFCLASS64		2

/**
 * @brief A structure to represent a single entry in /proc/self/maps.
 *
 * This structure holds information about a memory mapping, including its
 * start and end addresses, offset, inode, permissions, and the associated path.
 */
typedef struct {
	uintptr_t	start;
	uintptr_t	end;
	uintptr_t	offset;
	uintptr_t	inode;
	unsigned	rwx;		/* bit 0: r, 1: w, 2: x */
	char		path[LK_PATH_MAX];
} mapsEntry;

/**
 * @brief A structure to hold ELF information read from a file.
 *
 * This structure contains an array of program headers, the number of program
 * headers, and the ELF type (ET_EXEC or ET_DYN).
 */
typedef struct {
	Elf64_Phdr	phdrs[LK_MAX_PHDRS];
	int			nphdrs;
	uint16_t	e_type;		/* ELF_ET_EXEC or ELF_ET_DYN */
} elf_info;


/**
 * @brief Parse a string into an unsigned integer.
 * @param pp A pointer to a pointer to the string to parse.
 * @param out A pointer to the variable to store the parsed integer.
 * @return (0) on success, or -1 on failure.
 */
static int parseUint(const char **pp, uintptr_t *out)
{
	const char	*p = *pp;
	uintptr_t	v = 0;
	int			n = 0;
	int			base = 10;

	if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
		base = 16;
		p += 2;
	}
	while (*p) {
		int c = *p;
		int d;
		if (c >= '0' && c <= '9')		d = c - '0';
		else if (c >= 'a' && c <= 'f')	d = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')	d = c - 'A' + 10;
		else break;
		if (d >= base)
			break;
		v = v * (uintptr_t)base + (uintptr_t)d;
		p++;
		n++;
	}
	if (n == 0)
		return (-1);
	*out = v;
	*pp = p;
	return (0);
}

static int parseMapsLine(const char *p, mapsEntry *e)
{
	size_t i;

	memset(e, 0, sizeof(*e));

	if (parseUint(&p, &e->start) < 0)	return (-1);
	if (*p != '-')	return (-1);
	p++;
	if (parseUint(&p, &e->end) < 0)	return (-1);
	if (*p != ' ')	return (-1);
	p++;

	if (p[0] == 'r') e->rwx |= 1;
	if (p[1] == 'w') e->rwx |= 2;
	if (p[2] == 'x') e->rwx |= 4;
	p += 3;
	if (*p != ' ')	return (-1);
	p++;

	if (parseUint(&p, &e->offset) < 0)	return (-1);
	if (*p != ' ')	return (-1);
	p++;

	/* dev major:minor - skip */
	while (*p && *p != ' ') p++;
	if (*p != ' ')	return (-1);
	p++;

	if (parseUint(&p, &e->inode) < 0)	return (-1);
	while (*p == ' ') p++;

	i = 0;
	while (*p && *p != '\n' && i < LK_PATH_MAX - 1)
		e->path[i++] = *p++;
	e->path[i] = '\0';

	return (0);
}

/**
 * @brief Read the /proc/self/maps file and parse its entries.
 * @param buf A buffer to store the contents of the maps file.
 * @param bufSize The size of the buffer.
 * @param outEntries An array to store the parsed maps entries.
 * @param maxEntries The maximum number of entries to parse.
 * @return The number of entries parsed, or -1 on failure.
 */
static int readElfInfo(const char *path, elf_info *info)
{
	int			fd;
	Elf64_Ehdr	eh;
	ssize_t		n;
	int			phnum;

	memset(info, 0, sizeof(*info));

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return (-1);

	n = read(fd, &eh, sizeof(eh));
	if (n != (ssize_t)sizeof(eh))
		goto fail;

	if (memcmp(eh.e_ident, "\x7f" "ELF", 4) != 0)
		goto fail;
	if (eh.e_ident[4] != LK_ELFCLASS64)
		goto fail;
	if (eh.e_phentsize != sizeof(Elf64_Phdr))
		goto fail;

	info->e_type = eh.e_type;

	phnum = eh.e_phnum;
	if (phnum <= 0 || phnum > LK_MAX_PHDRS)
		phnum = (phnum <= 0 ? 0 : LK_MAX_PHDRS);

	if (phnum == 0) {
		close(fd);
		return (0);
	}

	if (lseek(fd, (off_t)eh.e_phoff, SEEK_SET) < 0)
		goto fail;

	for (int i = 0; i < phnum; i++) {
		n = read(fd, &info->phdrs[i], sizeof(Elf64_Phdr));
		if (n != (ssize_t)sizeof(Elf64_Phdr))
			goto fail;
	}
	info->nphdrs = phnum;
	close(fd);
	return (0);

fail:
	close(fd);
	return (-1);
}

/**
 * @brief Create a fake program header from a memory mapping entry.
 * @param e The mapsEntry representing the memory mapping.
 * @param ph The Elf64_Phdr to populate with the fake program header.
 */
static void fakePhdrFromEntry(const mapsEntry *e, Elf64_Phdr *ph)
{
	memset(ph, 0, sizeof(*ph));
	ph->p_type	= PT_LOAD;
	ph->p_flags	= 0;
	if (e->rwx & 1) ph->p_flags |= PF_R;
	if (e->rwx & 2) ph->p_flags |= PF_W;
	if (e->rwx & 4) ph->p_flags |= PF_X;
	ph->p_offset	= e->offset;
	ph->p_vaddr		= e->start;
	ph->p_paddr		= e->start;
	ph->p_filesz	= e->end - e->start;
	ph->p_memsz		= ph->p_filesz;
	ph->p_align		= 0x1000;
}

/**
 * @brief Emit a group of program headers to the callback.
 * @param cb The callback function to invoke.
 * @param data User-defined data to pass to the callback.
 * @param path The path of the ELF object.
 * @param entries An array of mapsEntry representing the memory mappings.
 * @param nentries The number of entries in the array.
 * @param loadBase The base address where the ELF object is loaded.
 * @return The return value from the callback, or 0 on success.
 */
static int emitGroup(int				(*cb)(struct dl_phdr_info *, size_t, void *),
					  void				*data,
					  const char		*path,
					  const mapsEntry	*entries, int nentries,
					  uintptr_t 		loadBase)
{
	Elf64_Phdr			fake[LK_MAX_PHDRS];
	struct dl_phdr_info	info;
	elf_info			ei;
	const Elf64_Phdr	*phdrs = NULL;
	int					nphdrs = 0;
	Elf64_Addr			addr = 0;

	if (readElfInfo(path, &ei) == 0 && ei.nphdrs > 0) {
		/* Real phdrs. ET_EXEC: vaddr is already absolute.
		 * ET_DYN (PIE / .so): runtime = loadBase + vaddr. */
		phdrs	= ei.phdrs;
		nphdrs	= ei.nphdrs;
		addr	= (ei.e_type == ELF_ET_EXEC) ? 0 : (Elf64_Addr)loadBase;
	} else {
		/* Fallback: one fake PT_LOAD per file-backed mapping. */
		int n = nentries < LK_MAX_PHDRS ? nentries : LK_MAX_PHDRS;
		for (int i = 0; i < n; i++)
			fakePhdrFromEntry(&entries[i], &fake[i]);
		if (n == 0)
			return (0);
		phdrs	= fake;
		nphdrs	= n;
		addr	= 0;
	}

	memset(&info, 0, sizeof(info));
	info.dlpi_addr	= addr;
	info.dlpi_name	= path;
	info.dlpi_phdr	= phdrs;
	info.dlpi_phnum	= (Elf64_Half)nphdrs;

	return (cb(&info, sizeof(info), data));
}

int dl_iterate_phdr(int (*callback)(struct dl_phdr_info *, size_t, void *), void *data)
{
	static char	buf[LK_MAPS_BUF_SIZE];
	mapsEntry	group[LK_MAX_PHDRS];
	char		group_path[LK_PATH_MAX];
	int			ngroup = 0;
	uintptr_t	loadBase = 0;
	ssize_t		total = 0;
	ssize_t		n;
	int			fd;
	int			rc = 0;
	const char	*p;

	if (callback == NULL)
		return (-1);

	fd = open("/proc/self/maps", O_RDONLY);
	if (fd < 0)
		return (-1);

	while (total < (ssize_t)sizeof(buf) - 1) {
		n = read(fd, buf + total, sizeof(buf) - 1 - (size_t)total);
		if (n <= 0)
			break;
		total += n;
	}
	close(fd);
	if (total <= 0)
		return (-1);
	buf[total] = '\0';

	group_path[0] = '\0';
	p = buf;

	while (*p) {
		const char	*eol = p;
		mapsEntry	e;
		int			ok;
		char		saved;

		while (*eol && *eol != '\n')
			eol++;

		saved = *eol;
		((char *)eol)[0] = '\0';
		ok = (parseMapsLine(p, &e) == 0);
		((char *)eol)[0] = saved;

		if (ok) {
			int is_file = (e.path[0] != '\0' && e.path[0] != '[');

			if (!is_file) {
				if (ngroup > 0) {
					rc = emitGroup(callback, data, group_path,
									group, ngroup, loadBase);
					ngroup = 0;
					group_path[0] = '\0';
					loadBase = 0;
					if (rc != 0)
						goto done;
				}
			} else if (ngroup > 0
					   && strcmp(e.path, group_path) == 0) {
				if (ngroup < LK_MAX_PHDRS) {
					group[ngroup++] = e;
					uintptr_t base = e.start - e.offset;
					if (base < loadBase)
						loadBase = base;
				}
			} else {
				if (ngroup > 0) {
					rc = emitGroup(callback, data, group_path,
									group, ngroup, loadBase);
					ngroup = 0;
					if (rc != 0)
						goto done;
				}
				size_t plen = strlen(e.path);
				if (plen >= LK_PATH_MAX)
					plen = LK_PATH_MAX - 1;
				memcpy(group_path, e.path, plen);
				group_path[plen] = '\0';

				group[ngroup++] = e;
				loadBase = e.start - e.offset;
			}
		}

		p = (*eol ? eol + 1 : eol);
	}

	if (ngroup > 0)
		rc = emitGroup(callback, data, group_path, group, ngroup, loadBase);

done:
	return rc;
}
