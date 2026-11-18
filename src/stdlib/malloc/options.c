/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file options.c
 * @brief Option parsing for the hajlibc allocator.
 * @Created: 2026/10/19 10:48:42 by Moutig
 * @Updated: 2026/11/08 03:39:28 by Moutig
 *
 * The allocator supports a small set of options that can be set
 * via environment variables or programmatically. The options are
 * stored in g_hmalloc.options, an array of atomic longs. The
 * options are initialized from the environment at process startup
 * and can be modified at runtime.
 */

#include "include/hmInternal.h"
#include "include/hmPrivate.h"

/**
 * @brief Get the atomic slot for an option.
 * @param opt The option to get the slot for.
 * @return A pointer to the atomic slot for the option.
 */
static inline _Atomic long	*hmOptSlot(t_hmallocOption opt)
{
	return ((_Atomic long *)&g_hmalloc.options[opt]);
}

/**
 * @brief Store a value in an option slot.
 * @param opt The option to store the value in.
 * @param v The value to store.
 */
static inline void	hmOptStore(t_hmallocOption opt, long v)
{
	atomic_store_explicit(hmOptSlot(opt), v, memory_order_relaxed);
}

/**
 * @brief Load a value from an option slot.
 * @param opt The option to load the value from.
 * @return The value loaded from the option slot.
 */
static inline long	hmOptLoad(t_hmallocOption opt)
{
	return (atomic_load_explicit(hmOptSlot(opt), memory_order_relaxed));
}

typedef struct s_hmOptionDef {
	t_hmallocOption	opt;
	const char		*env;
	long			def;
} t_hmOptionDef;

/* 42 build: freed zones go back to the system at once, no huge pages. */
#if HMALLOC_FT42
# define HMOPT_PURGE_DELAY_DEF	0
# define HMOPT_ALLOW_THP_DEF	0
#else
# define HMOPT_PURGE_DELAY_DEF	1000
# define HMOPT_ALLOW_THP_DEF	2
#endif

static const t_hmOptionDef	g_optionDefs[] = {
	{ HMALLOC_OPT_SHOW_ERRORS,			"HMALLOC_SHOW_ERRORS",			0 },
	{ HMALLOC_OPT_SHOW_STATS,			"HMALLOC_SHOW_STATS",			0 },
	{ HMALLOC_OPT_VERBOSE,				"HMALLOC_VERBOSE",				0 },
	{ HMALLOC_OPT_PURGE_DELAY,			"HMALLOC_PURGE_DELAY",			HMOPT_PURGE_DELAY_DEF },
	{ HMALLOC_OPT_PAGE_FULL_RETAIN,	"HMALLOC_PAGE_FULL_RETAIN",	2 },
	{ HMALLOC_OPT_GUARDED_SAMPLE_RATE,	"HMALLOC_GUARDED_SAMPLE_RATE",	0 },
	{ HMALLOC_OPT_PERTURB,				"HMALLOC_PERTURB",				0 },
	{ HMALLOC_OPT_CHECK,				"HMALLOC_CHECK",				0 },
	{ HMALLOC_OPT_CHECK_LEAKS,			"HMALLOC_CHECK_LEAKS",			0 },
	{ HMALLOC_OPT_PAGE_RECLAIM_ON_FREE,	"HMALLOC_PAGE_RECLAIM_ON_FREE",	0 },
	{ HMALLOC_OPT_PAGE_MAX_RECLAIM,		"HMALLOC_PAGE_MAX_RECLAIM",		-1 },
	{ HMALLOC_OPT_PAGE_CROSS_THREAD_MAX_RECLAIM,
		"HMALLOC_PAGE_CROSS_THREAD_MAX_RECLAIM",						32 },
	{ HMALLOC_OPT_GENERIC_COLLECT,		"HMALLOC_GENERIC_COLLECT",		10000 },
	{ HMALLOC_OPT_ARENA_PURGE_MULT,		"HMALLOC_ARENA_PURGE_MULT",		4 },
	{ HMALLOC_OPT_ALLOW_THP,			"HMALLOC_ALLOW_THP",			HMOPT_ALLOW_THP_DEF },
	{ HMALLOC_OPT_ARENA_PURGE_MIN_SIZE,	"HMALLOC_ARENA_PURGE_MIN_SIZE",	0 },
	/* 8 * HMALLOC_LARGE_PAGE_SIZE = 32 MiB, in KiB */
	{ HMALLOC_OPT_ARENA_PURGE_IMMEDIATE_SIZE,
		"HMALLOC_ARENA_PURGE_IMMEDIATE_SIZE",						32768 },
};

#define HMOPT_DEF_COUNT	(sizeof(g_optionDefs) / sizeof(g_optionDefs[0]))

_Static_assert(HMALLOC_OPT_LAST <= HMALLOC_OPTIONS_MAX,
	"HMALLOC_OPTIONS_MAX too small for t_hmallocOption");

/**
 * @brief Check if a character is a whitespace character.
 * @param c The character to check.
 * @return true if the character is a whitespace character, false otherwise.
 */
static bool	hmOptIsSpace(char c)
{
	return (c == ' ' || c == '\t' || c == '\n' || c == '\r'
			|| c == '\f' || c == '\v');
}

/**
 * @brief Compare two strings case-insensitively.
 * @param a The first string.
 * @param b The second string.
 * @return true if the strings are equal case-insensitively, false otherwise.
 */
static bool	hmOptStreqCI(const char *a, const char *b)
{
	while (*a != '\0' && *b != '\0') {
		char	ca = *a;
		char	cb = *b;

		if (ca >= 'A' && ca <= 'Z')
			ca = (char)(ca - 'A' + 'a');
		if (cb >= 'A' && cb <= 'Z')
			cb = (char)(cb - 'A' + 'a');
		if (ca != cb)
			return (false);
		a++;
		b++;
	}
	return (*a == '\0' && *b == '\0');
}

/**
 * @brief Parse a string as a long integer.
 *
 * The string can be a decimal, hexadecimal (0x prefix), or binary (0b prefix) number.
 * It can also be a boolean value ("true", "false", "on", "off", "yes", "no", "y", "n").
 * Leading and trailing whitespace is ignored. If the string is not a valid number or boolean,
 * the function returns 0 and sets *ok to false.
 * @param s The string to parse.
 * @param ok A pointer to a boolean that will be set to true if the parsing was successful, false otherwise.
 * @return The parsed long integer, or 0 if the parsing failed.
 */
static long	hmOptParse(const char *s, bool *ok)
{
	long	v = 0;
	bool	neg = false;
	int		base = 10;

	*ok = false;

	/* Ignore leading whitespace. */
	while (hmOptIsSpace(*s))
		s++;
	if (*s == '\0')
		return (0);

	/* Boolean values. */
	if (hmOptStreqCI(s, "true")  || hmOptStreqCI(s, "on")
		|| hmOptStreqCI(s, "yes") || hmOptStreqCI(s, "y"))
	{
		*ok = true;
		return (1);
	}
	if (hmOptStreqCI(s, "false") || hmOptStreqCI(s, "off")
		|| hmOptStreqCI(s, "no")  || hmOptStreqCI(s, "n"))
	{
		*ok = true;
		return (0);
	}

	/* Sign. */
	if (*s == '-') {
		neg = true;
		s++;
	} else if (*s == '+') {
		s++;
	}

	/* Base. */
	if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		base = 16;
		s += 2;
	} else if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) {
		base = 2;
		s += 2;
	}

	if (*s == '\0')
		return (0);

	while (*s != '\0' && !hmOptIsSpace(*s)) {
		int		d;
		char	c = *s;

		if (c >= '0' && c <= '9')
			d = c - '0';
		else if (c >= 'a' && c <= 'f')
			d = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			d = c - 'A' + 10;
		else
			return (0);

		if (d >= base)
			return (0);

		v = v * base + d;
		s++;
	}

	if (neg)
		v = -v;
	*ok = true;
	return (v);
}

void	_hmallocOptionsInit(void)
{
	size_t	i;

	for (i = 0; i < HMOPT_DEF_COUNT; i++) {
		const t_hmOptionDef	*d = &g_optionDefs[i];
		char				buf[64];
		bool				ok;
		long				v;

		hmOptStore(d->opt, d->def);

		if (hmallocPrimGetenv(d->env, buf, sizeof(buf)) != 0)
			continue;

		v = hmOptParse(buf, &ok);
		if (!ok)
			continue;

		hmOptStore(d->opt, v);
	}
}

void	hmOptionSet(t_hmallocOption opt, long value)
{
	if ((int)opt < 0 || opt >= HMALLOC_OPT_LAST)
		return;
	hmOptStore(opt, value);
}

long	hmOptionGet(t_hmallocOption opt)
{
	if ((int)opt < 0 || opt >= HMALLOC_OPT_LAST)
		return (0);
	return (hmOptLoad(opt));
}

size_t	hmOptionGetSize(t_hmallocOption opt)
{
	const long	x = hmOptionGet(opt);
	size_t		size = (x < 0) ? 0 : (size_t)x;

	/* Size options are kept in KiB. */
	return ((size > SIZE_MAX / 1024u) ? SIZE_MAX : size * 1024u);
}
