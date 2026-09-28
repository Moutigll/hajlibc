/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file errno.c
 * @brief Storage for the errno variable.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 18:27:18 by Moutig
 *
 * This file defines the thread-local errno variable declared in
 * <errno.h>. It must be compiled once and linked into the library.
 */

#include <errno.h>

__HAJ_THREAD_LOCAL int	__haj_errno = 0;
