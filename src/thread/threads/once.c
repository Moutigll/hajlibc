/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file once.c
 * @brief Implementation of call_once() and once_flag.
 * @Created: 2026/10/05 11:33:46 by Moutig
 * @Updated: 2026/10/05 12:09:46 by Moutig
 *
 * once_flag is pthread_once_t and call_once is a direct
 * wrapper over pthread_once. Both are 0-initialized to mean
 * "not yet run".
 */

#include <threads.h>
#include <pthread.h>

void call_once(once_flag *flag, void (*func)(void))
{
	if (flag == NULL || func == NULL)
		return;
	pthread_once(flag, func);
}
