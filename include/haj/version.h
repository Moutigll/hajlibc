/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file version.h
 * @brief hajlib version macros.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 16:13:10 by Moutig
 *
 * This header defines the hajlib version macros, including
 * HAJ_VERSION_MAJOR, HAJ_VERSION_MINOR, HAJ_VERSION_PATCH,
 * HAJ_VERSION_STRING, and HAJ_VERSION_NUM.
 */

#ifndef HAJ_VERSION_H
# define HAJ_VERSION_H

/**
 * hajlib version.
 * Follows Semantic Versioning 2.0.0 (https://semver.org).
 *
 * MAJOR : incremented on incompatible API changes.
 * MINOR : incremented on backward-compatible additions.
 * PATCH : incremented on backward-compatible bug fixes.
 *
 * While MAJOR is 0, the API is considered unstable: breaking
 * changes are allowed on MINOR bumps. See README for details.
 */

# define HAJ_VERSION_MAJOR	0
# define HAJ_VERSION_MINOR	3
# define HAJ_VERSION_PATCH	0

/**
 * String helpers: expand a macro to its value as a string.
 */
# define HAJ_STR_(x)	#x
# define HAJ_STR(x)	HAJ_STR_(x)

/**
 * Version string, built at compile time.
 * Example: "0.1.0"
 */
# define HAJ_VERSION_STRING \
	HAJ_STR(HAJ_VERSION_MAJOR) "." \
	HAJ_STR(HAJ_VERSION_MINOR) "." \
	HAJ_STR(HAJ_VERSION_PATCH)

/**
 * Numeric form for comparisons:
 *   (MAJOR * 10000) + (MINOR * 100) + PATCH
 * Example: 1.2.3 -> 10203
 */
# define HAJ_VERSION_NUM \
	((HAJ_VERSION_MAJOR * 10000) + \
	 (HAJ_VERSION_MINOR * 100)   + \
	 (HAJ_VERSION_PATCH))

#endif /* HAJ_VERSION_H */
