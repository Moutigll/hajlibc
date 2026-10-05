/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file test.c
 * @brief Test framework implementation.
 * @Created: 2026/09/30 07:18:00 by Moutig
 * @Updated: 2026/09/30 11:08:37 by Moutig
 *
 * This file implements a simple test framework for hajlibc.
 * It provides functions to register and run test cases, as well as
 * to report the results of the tests. The framework supports filtering
 * test cases by name and categorizing them for better organization.
 */

#include "test.h"
#include <unistd.h>

static void put(const char *s)
{
	size_t n = 0;
	while (s[n]) n++;
	if (n) {
		ssize_t r = write(1, s, n);
		(void)r;
	}
}

static void putErr(const char *s)
{
	size_t n = 0;
	while (s[n]) n++;
	if (n) {
		ssize_t r = write(2, s, n);
		(void)r;
	}
}


static void putU64(unsigned long long v, int fd)
{
	char	buf[21];
	int		i = 20;
	buf[i] = '\0';
	if (v == 0) { buf[--i] = '0'; }
	while (v) { buf[--i] = (char)('0' + (v % 10)); v /= 10; }
	if (fd == 2)	putErr(&buf[i]);
	else			put(&buf[i]);
}

static void putI64(long long v, int fd)
{
	if (v < 0) {
		if (fd == 2)	putErr("-");
		else			put("-");
		/* -INT64_MIN est UB, on passe par unsigned */
		putU64((unsigned long long)(-(v + 1)) + 1ULL, fd);
	} else {
		putU64((unsigned long long)v, fd);
	}
}

void tfLog(const char *s)				{ put(s); }
void tfLogU64(unsigned long long v)	{ putU64(v, 1); }
void tfLogI64(long long v)			{ putI64(v, 1); }

int tfStreq(const char *a, const char *b)
{
	if (!a || !b) return a == b;
	while (*a && *a == *b) { a++; b++; }
	return *a == *b;
}

static int strContains(const char *str, const char *substr)
{
	if (!str || !substr) return (0);
	while (*str) {
		const char *s1 = str;
		const char *s2 = substr;
		while (*s1 && *s2 && *s1 == *s2) { s1++; s2++; }
		if (*s2 == '\0') return (1); /* found */
		str++;
	}
	return (0); /* not found */
}

/* ----- Registration of test cases ----- */

int g_pass = 0;
int g_fail = 0;

static testCase_t *head = NULL;
static testCase_t *tail = NULL;

#define MAX_CATEGORIES 64

static catStat_t	g_cats[MAX_CATEGORIES];
static int			g_catCount = 0;

static catStat_t *catGet(const char *name)
{
	for (int i = 0; i < g_catCount; i++) {
		if (tfStreq(g_cats[i].name, name)) return &g_cats[i];
	}
	if (g_catCount >= MAX_CATEGORIES) return &g_cats[0]; /* fallback */
	g_cats[g_catCount].name = name;
	g_cats[g_catCount].pass = 0;
	g_cats[g_catCount].fail = 0;
	return &g_cats[g_catCount++];
}

void testRegister(const char *category, const char *name, testFn_t fn)
{
	static testCase_t	pool[1024];
	static int			pool_idx = 0;

	if (pool_idx >= 1024) {
		putErr("testRegister: pool exhausted\n");
		_exit(2);
	}
	testCase_t *tc	= &pool[pool_idx++];
	tc->category	= category;
	tc->name		= name;
	tc->fn			= fn;
	tc->next		= NULL;

	if (tail)	tail->next = tc;
	else		head = tc;
	tail = tc;

	(void)catGet(category);
}

/* ----- Assertions ----- */

static void reportFailureHeader(const char *file, int line)
{
	putErr("	");
	putErr(file);
	putErr(":");
	putI64(line, 2);
	putErr(": ");
}

void tfFail(const char *file, int line, const char *expr)
{
	g_fail++;
	reportFailureHeader(file, line);
	putErr(expr);
	putErr(" failed\n");
}

void tfFailEq(const char *file, int line, const char *ea, const char *eb, long long a, long long b)
{
	g_fail++;
	reportFailureHeader(file, line);
	putErr("ASSERT_EQ(");
	putErr(ea); putErr(", "); putErr(eb);
	putErr("): ");
	putI64(a, 2); putErr(" != "); putI64(b, 2);
	putErr("\n");
}

void tfFailStreq(const char *file,	int			line,
				 const char *ea,	const char	*eb,
				 const char *a,		const char	*b)
{
	g_fail++;
	reportFailureHeader(file, line);
	putErr("ASSERT_STREQ(");
	putErr(ea); putErr(", "); putErr(eb);
	putErr("): \"");
	putErr(a ? a : "(null)");
	putErr("\" != \"");
	putErr(b ? b : "(null)");
	putErr("\"\n");
}

/* ---- Execution ---- */

static int countTotal(void)
{
	int n = 0;
	for (testCase_t *tc = head; tc; tc = tc->next) n++;
	return (n);
}

int testRunAll(const char *filter)
{
	int total	= countTotal();
	int matched	= 0;

	for (testCase_t *tc = head; tc; tc = tc->next) {
		if (filter && !strContains(tc->name, filter))
			continue;
		matched++;

		int before = g_fail;
		tc->fn();

		catStat_t *cs = catGet(tc->category);
		if (g_fail == before) {
			g_pass++;
			cs->pass++;
			put(C_GREEN"  PASS  "C_RESET);
		} else {
			cs->fail++;
			put(C_RED"  FAIL  "C_RESET);
		}
		put(C_CYAN);
		put(tc->category);
		put(C_RESET);
		put("/");
		put(tc->name);
		put("\n");
	}

	put("\n");
	testPrintReport();

	put(C_GRAY);
	put("Discovered: ");
	putI64(total, 1);
	put(", selected: ");
	putI64(matched, 1);
	put(C_RESET"\n");

	return ((g_fail == 0) ? 0 : 1);
}

/* ----- Reporting ---- */

static void putPadded(const char *s, int width)
{
	int n = 0;
	while (s[n]) n++;
	put(s);
	for (int i = n; i < width; i++) put(" ");
}

static void putU64Padded(unsigned long long v, int width)
{
	char	buf[21];
	int		i = 20;
	buf[i] = '\0';
	if (v == 0) { buf[--i] = '0'; }
	while (v) { buf[--i] = (char)('0' + (v % 10)); v /= 10; }
	int n = 20 - i;
	for (int k = n; k < width; k++) put(" ");
	put(&buf[i]);
}

static void printPercent(int pass, int total)
{
	if (total == 0) { put("  n/a"); return; }
	unsigned long long p100 = (unsigned long long)pass * 1000ULL
							/ (unsigned long long)total;
	putU64Padded(p100 / 10, 4);
	put(".");
	putU64Padded(p100 % 10, 1);
	put("%");
}

void testPrintReport(void)
{
	put(C_BOLD"=== Summary ===\n"C_RESET);

	put("  ");
	putPadded("category", 14);
	putPadded("run", 6);
	putPadded("pass", 6);
	putPadded("fail", 6);
	putPadded("pass%", 7);
	put("\n");

	int total_run = 0;
	for (int i = 0; i < g_catCount; i++) {
		int run = g_cats[i].pass + g_cats[i].fail;
		total_run += run;

		put("  "C_CYAN);
		putPadded(g_cats[i].name, 14);
		put(C_RESET);

		putU64Padded((unsigned long long)run, 6);

		put(C_GREEN);
		putU64Padded((unsigned long long)g_cats[i].pass, 6);
		put(C_RESET);

		if (g_cats[i].fail > 0)
			put(C_RED);
		putU64Padded((unsigned long long)g_cats[i].fail, 6);
		put(C_RESET);

		if (run == 0) {
			put("  n/a");
		} else if (g_cats[i].fail == 0) {
			put(C_GREEN);
			printPercent(g_cats[i].pass, run);
			put(C_RESET);
		} else {
			put(C_RED);
			printPercent(g_cats[i].pass, run);
			put(C_RESET);
		}
		put("\n");
	}

	/* Ligne de séparation. */
	put(C_GRAY);
	put("  ---------------------------------------------\n");
	put(C_RESET);

	/* Total. */
	put("  ");
	put(C_BOLD);
	putPadded("TOTAL", 14);
	put(C_RESET);
	putU64Padded((unsigned long long)total_run, 6);

	put(C_GREEN);
	putU64Padded((unsigned long long)g_pass, 6);
	put(C_RESET);

	if (g_fail > 0)
		put(C_RED);
	putU64Padded((unsigned long long)g_fail, 6);
	put(C_RESET);

	if (total_run == 0) {
		put("  n/a");
	} else if (g_fail == 0) {
		put(C_GREEN);
		printPercent(g_pass, total_run);
		put(C_RESET);
	} else {
		put(C_RED);
		printPercent(g_pass, total_run);
		put(C_RESET);
	}
	put("\n\n");

	if (g_fail == 0) {
		put(C_BOLD);
		put(C_GREEN);
		put("ALL TESTS PASSED");
		put(C_RESET);
		put(" (");
		putI64(g_pass, 1);
		put(" tests)\n");
	} else {
		put(C_BOLD);
		put(C_RED);
		put("TESTS FAILED");
		put(C_RESET);
		put(" (");
		putI64(g_pass, 1);
		put(" passed, ");
		putI64(g_fail, 1);
		put(" failed)\n");
	}
}
