/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file main.c
 * @brief Test program for hajlib.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/30 11:09:55 by Moutig
 *
 * This file contains the main function for the test program of hajlibc.
 */

#include "framework/test.h"

int main(int argc, char **argv)
{
	const char *filter = (argc > 1) ? argv[1] : NULL;
	return testRunAll(filter);
}
