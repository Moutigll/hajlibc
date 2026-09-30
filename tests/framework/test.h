/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file test.h
 * @brief Test framework header.
 * @Created: 2026/09/30 07:17:24 by Moutig
 * @Updated: 2026/09/30 11:09:46 by Moutig
 *
 * This header file defines the interface for a simple test framework for hajlibc.
 * It provides macros and functions to register and run test cases, as well as
 * to report the results of the tests. The framework supports filtering
 * test cases by name and categorizing them for better organization.
 */


#ifndef LIBC_TESTS_TEST_H
#define LIBC_TESTS_TEST_H

#include <stddef.h>

/**
 * @brief Type for test functions.
 * @param void
 * @return void
 */
typedef void (*testFn_t)(void);

/**
 * @brief Structure for a test case.
 * @param category Category of the test (e.g. "string", "ctype").
 * @param name Name of the test (e.g. "strlen", "isdigit").
 * @param fn Pointer to the test function.
 * @param next Pointer to the next test case in the linked list.
 */
typedef struct s_testCase {
	const char			*category;
	const char			*name;
	testFn_t			fn;
	struct s_testCase	*next;
} testCase_t;

/**
 * @brief Structure for category statistics.
 * @param name Name of the category.
 * @param pass Number of passed tests in this category.
 * @param fail Number of failed tests in this category.
 */
typedef struct {
	const char	*name;
	int			pass;
	int			fail;
} catStat_t;


/**
 * @brief Function to register a test case.
 * @param category Category of the test (e.g. "string", "ctype").
 * @param name Name of the test (e.g. "strlen", "isdigit").
 * @param fn Pointer to the test function.
 */
void	testRegister(const char *category, const char *name, testFn_t fn);

/**
 * @brief Run all registered tests, optionally filtered by category.
 * @param filter If non-NULL, only run tests whose category contains this substring.
 * @return 0 if all tests passed, 1 if any test failed.
 */
int		testRunAll(const char *filter);

/**
 * @brief Print a summary report of the test results.
 */
void	testPrintReport(void);

/**
 * @brief Macro to define a test case.
 * @param category Category of the test (e.g. "string", "ctype").
 * @param name Name of the test (e.g. "strlen", "isdigit").
 *
 * This macro defines a static function with the given name, and
 * registers it as a test case in the given category. The function
 * will be called when the tests are run.
 */
#define TEST(category, name)										\
	static void __haj_test_##category##_##name(void);				\
	static void __attribute__((constructor))						\
	__haj_ctor_##category##_##name(void) {							\
		testRegister(#category, #name,								\
					 __haj_test_##category##_##name);				\
	}																\
	static void __haj_test_##category##_##name(void)

/**
 * @brief Log a string to the test output.
 * @param s The string to log.
 */
void tfLog(const char *s);

/**
 * @brief Log an unsigned 64-bit integer to the test output.
 * @param v The value to log.
 */
void tfLogU64(unsigned long long v);

/**
 * @brief Log a signed 64-bit integer to the test output.
 * @param v The value to log.
 */
void tfLogI64(long long v);

#define ASSERT(cond) do {									\
	if (!(cond)) {											\
		tfFail(__FILE__, __LINE__, "ASSERT(" #cond ")");	\
	}														\
} while (0)

#define ASSERT_EQ(a, b) do {							\
	long long _a = (long long)(a);						\
	long long _b = (long long)(b);						\
	if (_a != _b) {										\
		tfFailEq(__FILE__, __LINE__, #a, #b, _a, _b);	\
	}													\
} while (0)

#define ASSERT_STREQ(a, b) do {								\
	const char *_a = (const char *)(a);						\
	const char *_b = (const char *)(b);						\
	if (!tfStreq(_a, _b)) {									\
		tfFailStreq(__FILE__, __LINE__, #a, #b, _a, _b);	\
	}														\
} while (0)

#define ASSERT_NULL(p)	 ASSERT((p) == NULL)
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)

/**
 * @brief Fail the current test with a message.
 * @param file The source file where the failure occurred.
 * @param line The line number where the failure occurred.
 * @param expr The expression that failed.
 */
void	tfFail(const char *file, int line, const char *expr);

/**
 * @brief Fail the current test with a message for ASSERT_EQ.
 * @param file The source file where the failure occurred.
 * @param line The line number where the failure occurred.
 * @param ea The string representation of the first expression.
 * @param eb The string representation of the second expression.
 * @param a The value of the first expression.
 * @param b The value of the second expression.
 */
void	tfFailEq(const char *file, int line, const char *ea, const char *eb, long long a, long long b);
/**
 * @brief Fail the current test with a message for ASSERT_STREQ.
 * @param file The source file where the failure occurred.
 * @param line The line number where the failure occurred.
 * @param ea The string representation of the first string.
 * @param eb The string representation of the second string.
 * @param a The value of the first string.
 * @param b The value of the second string.
 */
void	tfFailStreq(const char *file, int line, const char *ea, const char *eb, const char *a, const char *b);

/**
 * @brief Compare two strings for equality, handling NULL pointers.
 * @param a The first string.
 * @param b The second string.
 * @return 1 if the strings are equal, 0 otherwise.
 */
int		tfStreq(const char *a, const char *b);
# define HAJ_USE_COLOR 1
# if HAJ_USE_COLOR

#define C_RESET		"\033[0m"
#define C_BOLD		"\033[1m"
#define C_RED		"\033[31m"
#define C_GREEN		"\033[32m"
#define C_YELLOW	"\033[33m"
#define C_CYAN		"\033[36m"
#define C_GRAY		"\033[90m"

# else

#  define C_RESET	""
#  define C_BOLD	""
#  define C_RED		""
#  define C_GREEN	""
#  define C_YELLOW	""
#  define C_CYAN	""
#  define C_GRAY	""

# endif

#endif /* LIBC_TESTS_TEST_H */
