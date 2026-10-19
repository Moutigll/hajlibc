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
 * @Updated: 2026/10/19 10:48:50 by Moutig
 *
 * Les options sont stockées dans g_hmalloc.options[] (tableau
 * atomique de HMALLOC_OPT_LAST long). Elles sont lues par le
 * chemin lent uniquement ; le chemin rapide ne consulte rien.
 *
 * Ordre d'init :
 *   1. _hmallocOptionsInit : lit les variables d'environnement,
 *      remplit g_hmalloc.options[]. Appelé sous g_hmalloc.lock
 *      par _hmallocAutoProcessInit, AVANT que la moindre erreur
 *      ne puisse être émise.
 *   2. _hmallocOptionsPostInit : ajuste les options qui dépendent
 *      de l'état du reste (ex. ARENA_RESERVE borné par addrLimit).
 *
 * Parsing :
 *   - Entier décimal, hexadécimal (0x…), binaire (0b…).
 *   - Booléens : "1" "true" "on" "yes" "y" → 1 ;
 *                "0" "false" "off" "no" "n" → 0.
 *   - Toute autre valeur est rejetée (option laissée à sa défaut).
 *
 * Aucune allocation : tout tient dans un tampon pile de 64 octets.
 *
 * Note atomics
 * ------------
 * Les entrées de g_hmalloc.options[] sont de type _Atomic long.
 * Certains compilateurs (et le <stdatomic.h> embarqué de hajlibc)
 * refusent un pointeur vers un lvalue qui n'est pas explicitement
 * _Atomic, même quand la déclaration du tableau l'est. On passe
 * donc systématiquement par un pointeur _Atomic long * obtenu
 * par un cast explicite, ce qui ne change rien au code généré :
 * l'objet pointé est bien le tableau atomique.
 */

#include "include/hmInternal.h"
#include "include/hmPrivate.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ---------- Accès atomique aux options ---------- */

/**
 * @brief Pointeur atomique vers l'entrée d'option d'index `i`.
 *
 * Le cast est sûr : g_hmalloc.options est déclaré `_Atomic long`.
 * On l'isole ici pour que le reste du fichier n'ait pas à
 * répéter la gymnastique.
 */
static inline _Atomic long	*hmOptSlot(t_hmallocOption opt)
{
	return ((_Atomic long *)&g_hmalloc.options[opt]);
}

static inline void	hmOptStore(t_hmallocOption opt, long v)
{
	atomic_store_explicit(hmOptSlot(opt), v, memory_order_relaxed);
}

static inline long	hmOptLoad(t_hmallocOption opt)
{
	return (atomic_load_explicit(hmOptSlot(opt), memory_order_relaxed));
}

/* ---------- Table des options ---------- */

typedef struct s_hmOptionDef {
	t_hmallocOption	opt;
	const char		*env;
	long			def;
} t_hmOptionDef;

static const t_hmOptionDef	g_optionDefs[] = {
	{ HMALLOC_OPT_SHOW_ERRORS,         "HMALLOC_SHOW_ERRORS",         0 },
	{ HMALLOC_OPT_SHOW_STATS,          "HMALLOC_SHOW_STATS",          0 },
	{ HMALLOC_OPT_VERBOSE,             "HMALLOC_VERBOSE",             0 },
	{ HMALLOC_OPT_PURGE_DELAY,         "HMALLOC_PURGE_DELAY",         0 },
	{ HMALLOC_OPT_ARENA_RESERVE,       "HMALLOC_ARENA_RESERVE",       0 },
	{ HMALLOC_OPT_PAGE_FULL_RETAIN,    "HMALLOC_PAGE_FULL_RETAIN",    0 },
	{ HMALLOC_OPT_GUARDED_SAMPLE_RATE, "HMALLOC_GUARDED_SAMPLE_RATE", 0 },
	{ HMALLOC_OPT_PERTURB,             "HMALLOC_PERTURB",             0 },
	{ HMALLOC_OPT_CHECK,               "HMALLOC_CHECK",               0 },
	{ HMALLOC_OPT_TRACE,               "HMALLOC_TRACE",               0 },
};

#define HMOPT_DEF_COUNT	(sizeof(g_optionDefs) / sizeof(g_optionDefs[0]))

/* ---------- Parseurs ---------- */

static bool	hmOptIsSpace(char c)
{
	return (c == ' ' || c == '\t' || c == '\n' || c == '\r'
			|| c == '\f' || c == '\v');
}

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
 * @brief Parse un entier ou un booléen.
 *
 * Supporte : décimal, 0x / 0X (hex), 0b / 0B (binaire), et les
 * alias booléens true/false/on/off/yes/no/y/n (insensibles à la
 * casse). Toute autre entrée laisse *ok à false.
 *
 * Ne détecte pas les overflows : une valeur absurde est tronquée
 * silencieusement. Les options n'ont pas de sensibilité à la
 * précision (ce sont des tailles en octets/KiB ou des flags).
 */
static long	hmOptParse(const char *s, bool *ok)
{
	long	v = 0;
	bool	neg = false;
	int		base = 10;

	*ok = false;

	/* Trim avant. */
	while (hmOptIsSpace(*s))
		s++;
	if (*s == '\0')
		return (0);

	/* Booléens textuels. */
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

	/* Signe. */
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

/* ---------- Init ---------- */

/**
 * @brief Lit chaque variable d'environnement et remplit options[].
 *
 * Appelé sous g_hmalloc.lock par _hmallocAutoProcessInit, avant
 * toute opération susceptible d'émettre un message d'erreur.
 * N'utilise pas getenv (non autorisé / peut malloc) : passe par
 * hmallocPrimGetenv.
 *
 * Les valeurs par défaut de la table g_optionDefs sont appliquées
 * en premier, puis écrasées par l'environnement si présent et
 * valide.
 */
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

/**
 * @brief Ajuste les options qui dépendent de l'état global.
 *
 * Exemple : ARENA_RESERVE doit rester strictement inférieur à
 * addrLimit, sinon les mmap échouent en ENOMEM dès la première
 * arène. On borne ici.
 */
void	_hmallocOptionsPostInit(void)
{
	long	v;
	size_t	limit;

	v = hmOptLoad(HMALLOC_OPT_ARENA_RESERVE);
	if (v <= 0)
		return;

	limit = g_hmalloc.addrLimit;
	if (limit == 0 || limit == SIZE_MAX)
		return;

	/*
	 * ARENA_RESERVE est exprimée en KiB par convention. On la
	 * plafonne à addrLimit / 16 pour laisser de la marge aux
	 * autres allocations (métadonnées, LARGE, mmap utilisateur).
	 */
	{
		size_t	bytes = (size_t)v * 1024;
		size_t	cap = limit / 16;

		if (bytes > cap) {
			long	kv = (long)(cap / 1024);

			if (kv < 1)
				kv = 1;
			hmOptStore(HMALLOC_OPT_ARENA_RESERVE, kv);
		}
	}
}

/* ---------- API publique ---------- */

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
