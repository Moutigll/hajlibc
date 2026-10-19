/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file profile.c
 * @brief Profiler for the hajlibc allocator.
 * @Created: 2026/10/19 10:46:23 by Moutig
 * @Updated: 2026/10/19 10:46:32 by Moutig
 *
 *
 * Le profileur repose sur un compteur `sampleCountdown` dans
 * chaque theap. Sous #if HMALLOC_PROFILE, le chemin d'allocation
 * fait :
 *
 *     if (hmUnlikely(th->sampleRate != 0
 *                    && size >= th->sampleCountdown)) {
 *         // slow path : appelle onSample
 *         th->sampleCountdown = th->sampleRate;
 *     } else if (th->sampleRate != 0) {
 *         th->sampleCountdown -= size;
 *     }
 *
 * Quand HMALLOC_PROFILE == 0, ce bloc disparaît ; ce fichier reste
 * compilé mais ses fonctions ne sont appelées par personne.
 *
 * Le [FLAG] du TODO signalait qu'aucun champ `profiler` n'existait
 * dans le heap : on l'a ajouté (`struct s_hmallocHeap.profiler`),
 * ainsi qu'un champ `heap` dans `t_hmallocProfiler` pour que
 * Start/Stop retrouvent le heap attaché.
 *
 * Verrouillage : les opérations Attach / Disable / Start / Stop
 * prennent le lock global (g_hmallocTs.lock). Le fast path lit
 * `th->sampleRate` sans lock (valeur immuable tant que le
 * profiler est actif).
 *
 * Une seule globale pour l'allocation : `g_hmalloc.heapMain`.
 * Une seule globale pour le thread-safe : `g_hmallocTs`.
 */

#include "include/hmInternal.h"
#include "include/hmPrivate.h"
#include "include/hmPrimTls.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ---------- Attach / detach ---------- */

/**
 * @brief Associe un profileur à un heap.
 *
 * Refuse si le heap a déjà un profileur différent. Un même
 * profileur peut être attaché à un seul heap à la fois.
 */
bool	hmHeapProfile(t_hmallocHeap *heap, t_hmallocProfiler *prof)
{
	if (heap == NULL || prof == NULL)
		return (false);

	hmallocLockAcquire(&g_hmallocTs.lock);

	if (heap->profiler != NULL && heap->profiler != prof) {
		hmallocLockRelease(&g_hmallocTs.lock);
		return (false);
	}

	heap->profiler = prof;
	prof->heap = heap;

	hmallocLockRelease(&g_hmallocTs.lock);
	return (true);
}

/**
 * @brief Détache le profileur du heap.
 *
 * Si un profileur était attaché, il est marqué inactif et son
 * champ `heap` est remis à NULL. Le heap est laissé propre.
 */
void	hmHeapProfileDisable(t_hmallocHeap *heap)
{
	if (heap == NULL)
		return;

	hmallocLockAcquire(&g_hmallocTs.lock);

	if (heap->profiler != NULL) {
		heap->profiler->active = false;
		heap->profiler->heap = NULL;
		heap->profiler = NULL;
	}

	hmallocLockRelease(&g_hmallocTs.lock);
}

/**
 * @brief Attache le profileur au heap principal.
 *
 * Raccourci pour hmHeapProfile(&g_hmalloc.heapMain, prof).
 */
bool	hmProfile(t_hmallocProfiler *prof)
{
	return (hmHeapProfile(&g_hmalloc.heapMain, prof));
}

/* ---------- Start / Stop ---------- */

/**
 * @brief Active l'échantillonnage sur le heap attaché.
 *
 * Positionne `sampleRate` et `sampleCountdown` sur le theap
 * courant. Les autres theaps du même heap seront initialisés
 * paresseusement : au prochain slow path, si leur
 * sampleCountdown est à zéro et que le heap a un profiler
 * actif, alloc.c doit appeler ce code pour les armer (voir la
 * note en fin de fichier).
 *
 * Retourne false si prof est NULL, ou si l'attachement échoue.
 */
bool	hmProfilerStart(t_hmallocProfiler *prof)
{
	t_hmallocHeap	*heap;
	t_hmallocTheap	*th;

	if (prof == NULL)
		return (false);

	if (prof->initialSampleRate == 0)
		prof->initialSampleRate = 64 * 1024;	/* 64 KiB par défaut */

	heap = prof->heap;
	if (heap == NULL) {
		/* Pas de heap attaché : on prend le heap principal. */
		heap = &g_hmalloc.heapMain;
		if (!hmHeapProfile(heap, prof))
			return (false);
	}

	hmallocLockAcquire(&g_hmallocTs.lock);

	prof->sampleRate = prof->initialSampleRate;
	prof->sampleCountdown = prof->sampleRate;
	prof->sampleCount = 0;
	prof->active = true;

	/*
	 * Arme le theap courant s'il existe. Les autres theaps du
	 * heap seront armés paresseusement, au prochain slow path.
	 */
	th = hmallocTheapDefaultGet();
	if (th != NULL && th->tld != NULL) {
		th->sampleRate = prof->sampleRate;
		th->sampleCountdown = prof->sampleCountdown;
		th->profileSampleRate = prof->initialSampleRate;
	}

	hmallocLockRelease(&g_hmallocTs.lock);
	return (true);
}

/**
 * @brief Désactive l'échantillonnage.
 *
 * Remet le theap courant au repos (sampleRate = 0,
 * sampleCountdown = SIZE_MAX), détache le profileur du heap si
 * c'est bien le sien, et invoque le callback onStop.
 *
 * Le callback est appelé **hors du lock**, pour qu'il puisse
 * prendre d'autres locks sans risque d'inversion.
 */
bool	hmProfilerStop(t_hmallocProfiler *prof)
{
	t_hmallocHeap	*heap;
	t_hmallocTheap	*th;

	if (prof == NULL)
		return (false);

	heap = prof->heap;

	hmallocLockAcquire(&g_hmallocTs.lock);

	prof->active = false;
	prof->sampleCountdown = SIZE_MAX;	/* "jamais" côté fast path */

	th = hmallocTheapDefaultGet();
	if (th != NULL && th->tld != NULL) {
		th->sampleRate = 0;
		th->sampleCountdown = SIZE_MAX;
	}

	if (heap != NULL && heap->profiler == prof) {
		heap->profiler = NULL;
		prof->heap = NULL;
	}

	hmallocLockRelease(&g_hmallocTs.lock);

	if (prof->onStop != NULL)
		prof->onStop(prof->arg);
	return (true);
}

/*
 * Note d'intégration (alloc.c, chemin lent) :
 *
 *   #if HMALLOC_PROFILE
 *   if (th->sampleRate != 0) {
 *       if (size >= th->sampleCountdown) {
 *           t_hmallocProfiler *p = th->heap->profiler;
 *           if (p != NULL && p->active && p->onSample != NULL) {
 *               p->onSample(p->arg, block, requested, usable);
 *               p->sampleCount++;
 *               p->totalSamples++;
 *           }
 *           th->sampleCountdown = th->sampleRate;
 *       } else {
 *           th->sampleCountdown -= size;
 *       }
 *   }
 *   #endif
 *
 * Ce bloc n'est compilé que si HMALLOC_PROFILE = 1. Par défaut
 * (0), aucun coût sur le chemin rapide.
 *
 * Armer paresseusement les theaps créés après Start :
 *
 *   if (HMALLOC_PROFILE && th->sampleRate == 0
 *       && th->heap->profiler != NULL
 *       && th->heap->profiler->active) {
 *       th->sampleRate = th->heap->profiler->sampleRate;
 *       th->sampleCountdown = th->sampleRate;
 *   }
 *
 * Optionnel : évite qu'un thread créé après hmProfilerStart()
 * ne soit jamais échantillonné.
 */
