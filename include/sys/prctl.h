/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <moutig-tan@proton.me>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file prctl.h
 * @brief Process control operations (Linux).
 * @Created: 2026/11/15 21:57:24 by Moutig
 * @Updated: 2026/11/15 21:57:41 by Moutig
 *
 * This file contains the declarations for the prctl(2) system call.
 */

#ifndef _SYS_PRCTL_H
# define _SYS_PRCTL_H

# include <bits/compiler.h>

# ifdef __cplusplus
extern "C" {
# endif

/* Transparent huge pages for the whole process. */
# define PR_SET_THP_DISABLE	41
# define PR_GET_THP_DISABLE	42

/**
 * @brief Operate on the calling process (Linux).
 *
 * @param option The operation (PR_*).
 * @param ...    Up to four `unsigned long` arguments, depending on `option`.
 * @return Depends on `option` (>= 0), or -1 with errno set.
 */
int	prctl(int option, ...);

# ifdef __cplusplus
}
# endif

#endif /* _SYS_PRCTL_H */
