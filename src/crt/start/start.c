/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file start.c
 * @brief C runtime startup code.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/10/02 09:48:33 by Moutig
 *
 * This file contains the C runtime startup code. It is called by the
 * __start assembly entry point, which is the first code executed in a
 * new process. It sets up the C runtime state, runs constructors, and
 * calls main().
 */

#include <bits/crt.h>
#include <bits/cpu.h>
#include <bits/thread/tcb.h>
#include <stdlib.h>
#include <stddef.h>

/*
 * The program name, extracted from argv[0]. If argv[0] is NULL or empty,
 * __progname is set to "(program)".
 */
const char			*__progname = "(program)";

/*
 * The argument count, as passed to main().
 */
int					__haj_argc = 0;

/*
 * The environment pointer, as passed to main().
 */
char				**environ = 0;

/**
 * The auxiliary vector. This is a pointer to an array of
 * Elf64_auxv_t structures.
 */
const unsigned long	*__haj_auxv = 0;

void				*__dso_handle = 0;


/**
 * @brief Parse the initial stack layout.
 *
 * The initial stack layout is the Linux initial stack:
 * [argc][argv...][NULL][envp...][NULL][auxv...][NULL].
 *
 * @param spVoid    The stack pointer at process entry.
 * @param outArgc   Output: the argument count.
 * @param outArgv   Output: the argument vector.
 * @param outEnvp   Output: the environment pointer.
 * @param outAuxv   Output: the auxiliary vector.
 */
static void parseInitialStack(void				*spVoid,
								int				*outArgc,
								char			***outArgv,
								char			***outEnvp,
								unsigned long	**outAuvx)
{
	unsigned long	*sp = (unsigned long *)spVoid;
	unsigned long	argc;
	char			**argv;
	char			**envp;
	unsigned long	*auxv;

	argc = sp[0];
	argv = (char **)&sp[1];

	/*
	 * envp starts right after argv[argc] and the terminating
	 * NULL. So envp = argv + (argc + 1).
	 */
	envp = argv + argc + 1;

	/*
	 * auxv starts right after the terminating NULL of envp.
	 * Walk envp until the NULL, then skip it.
	 */
	auxv = (unsigned long *)envp;
	while (*auxv != 0)
		auxv++;
	auxv++;

	*outArgc = (int)argc;
	*outArgv = argv;
	*outEnvp = envp;
	*outAuvx = auxv;
}

void __hajlibcStartMain(void *sp, int (*main)(int, char **, char **))
{
	int				argc;
	char			**argv;
	char			**envp;
	unsigned long	*auxv;
	int				ret;

	/*
	 * Step 1: parse the initial stack.
	 */
	parseInitialStack(sp, &argc, &argv, &envp, &auxv);

	/*
	 * Step 2: publish the standard globals. These are read
	 * by user code through environ, __progname, and the
	 * internal __haj_argc / __haj_auxv.
	 */
	__haj_argc = argc;
	environ = envp;
	__progname = (argc > 0 && argv[0] != NULL) ? argv[0] : "";
	__haj_auxv = auxv;

	/*
	 * Step 3: detect CPU features and fill the internal cache.
	 * This must be done before any call to hajCpuHas*().
	 */
	hajCpuDetect();

	/*
	 * Step 4: install the main thread's TCB. From this point
	 * on, pthread_self() and any TCB access work.
	 */
	__hajSetThreadRegister();

	/*
	 * Step 5: run constructors. They may now use pthread_self() and TCB access.
	 */
	__haj_run_ctors();

	/*
	 * Step 6: call main.
	 */
	ret = main(argc, argv, envp);

	/*
	 * Step 7: exit with main's return value. exit() never
	 * returns, but if it does, we fall through and __hajlibc_
	 * start_main is declared __HAJ_NORETURN, so the compiler
	 * knows the code after this line is unreachable.
	 */
	exit(ret);
	__builtin_unreachable();
}
