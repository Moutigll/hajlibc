/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file clone.h
 * @brief clone(2) flags and helpers.
 * @Created: 2026/09/30 05:11:04 by Moutig
 * @Updated: 2026/10/01 13:49:46 by Moutig
 *
 * clone(2) is Linux-specific. It creates a new process,
 * optionally sharing parts of the execution context with the
 * parent. POSIX threads are created by calling clone() with the
 * right set of flags.
 *
 * The flags are part of the Linux kernel ABI and are stable.
 * FreeBSD (thr_new) and Darwin (bsdthread_create) do not have
 * clone(); the flags below are Linux-only.
 */

#ifndef _BITS_THREAD_CLONE_H
# define _BITS_THREAD_CLONE_H

# include <bits/os.h>
# include <bits/signal.h>

# if defined(HAJ_OS_LINUX)

/* ----- clone() flags ----- */

#  define CLONE_NEWTIME			0x00000080	/* Create a new time namespace */
#  define CLONE_VM				0x00000100	/* Share address space */
#  define CLONE_FS				0x00000200	/* Share cwd, root, umask */
#  define CLONE_FILES			0x00000400	/* Share the fd table */
#  define CLONE_SIGHAND			0x00000800	/* Share signal handlers */
#  define CLONE_PIDFD			0x00001000	/* Store PID in userfd */
#  define CLONE_PTRACE			0x00002000	/* Child is traced */
#  define CLONE_VFORK			0x00004000	/* Parent waits for child to exit or exec */
#  define CLONE_PARENT			0x00008000	/* Child shares parent */
#  define CLONE_THREAD			0x00010000	/* Same thread group */
#  define CLONE_NEWNS			0x00020000	/* New mount namespace */
#  define CLONE_SYSVSEM			0x00040000	/* Share SysV sem undo values */
#  define CLONE_SETTLS			0x00080000	/* Set TLS %fs/TPIDR_EL0 in the child */
#  define CLONE_PARENT_SETTID	0x00100000	/* Store TID in usertidptr */
#  define CLONE_CHILD_CLEARTID	0x00200000	/* Clear TID in child on exit */
#  define CLONE_DETACHED		0x00400000	/* Unused, ignored */
#  define CLONE_UNTRACED		0x00800000	/* Child is not traced */
#  define CLONE_CHILD_SETTID	0x01000000	/* Store TID in child_tidptr */
#  define CLONE_NEWCGROUP		0x02000000	/* New cgroup namespace */
#  define CLONE_NEWUTS			0x04000000	/* New UTS namespace */
#  define CLONE_NEWIPC			0x08000000	/* New IPC namespace */
#  define CLONE_NEWUSER			0x10000000	/* New user namespace */
#  define CLONE_NEWPID			0x20000000	/* New PID namespace */
#  define CLONE_NEWNET			0x40000000	/* New network namespace */
#  define CLONE_IO				0x80000000	/* Clone io context */

/* ----- clone3-only flags (64-bit) ----- */

/*
 * CLONE_INTO_CGROUP is the only flag that does not fit in a
 * 32-bit int. It is why clone3 uses a `unsigned long long` for
 * its flags field. On clone(2) it cannot be passed.
 */
#  define CLONE_INTO_CGROUP		0x200000000ULL

/**
 * @brief The set of flags that POSIX threads use.
 *
 * CLONE_VM             share address space
 * CLONE_FS             share cwd, root, umask
 * CLONE_FILES          share the fd table
 * CLONE_SIGHAND        share signal handlers
 * CLONE_THREAD         same thread group
 * CLONE_SYSVSEM        share SysV sem undo values
 * CLONE_SETTLS         set the child TLS (%fs / tpidr_el0)
 * CLONE_PARENT_SETTID  write the child TID into parent_tid
 * CLONE_CHILD_CLEARTID clear child_tid on exit and futex-wake
 * CLONE_CHILD_SETTID   store the child TID in child_tidptr
 *
 * CLONE_CHILD_CLEARTID is what makes pthread_join() possible:
 * when the child exits, the kernel clears the futex word and
 * wakes anyone waiting on it.
 */

#  define HAJ_CLONE_THREAD_FLAGS \
	(CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | \
	 CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS | \
	 CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID)

/* ----- clone3(2) ----- */

/**
 * @brief Published sizes of `struct clone_args`.
 *
 * The kernel accepts any size <= the current one, as long as
 * every field covered by that size is zero-initialized when
 * unknown. We always pass the full sizeof(struct hajCloneArgs).
 */
#  define CLONE_ARGS_SIZE_VER0	64
#  define CLONE_ARGS_SIZE_VER1	80
#  define CLONE_ARGS_SIZE_VER2	88

/**
 * @brief Arguments for clone3(2).
 *
 * Layout matches the kernel's `struct clone_args` exactly.
 * Every field is `unsigned long long` (aligned 8).
 */
struct hajCloneArgs {
	unsigned long long	flags;			/* CLONE_* flags (no exit signal) */
	unsigned long long	pidfd;			/* out: pidfd (CLONE_PIDFD) */
	unsigned long long	childTid;		/* out: TID in child (CLONE_CHILD_SETTID) */
	unsigned long long	parentTid;		/* out: TID in parent (CLONE_PARENT_SETTID) */
	unsigned long long	exitSignal;		/* signal sent to parent on exit */
	unsigned long long	stack;			/* child stack top (high address) */
	unsigned long long	stackSize;		/* child stack size (informational) */
	unsigned long long	tls;			/* TLS pointer (CLONE_SETTLS) */
	unsigned long long	setTid;			/* array of TIDs for nested PID ns */
	unsigned long long	setTidSize;		/* size of setTid array */
	unsigned long long	cgroup;			/* target cgroup (CLONE_INTO_CGROUP) */
};

/* ----- Raw syscall wrappers -----
 *
 * Both take the same C-level arguments:
 *   fn         function to run in the child
 *   stack      top of the child's stack (high address, will be aligned)
 *   flags      CLONE_* flags, WITH the exit signal in the low byte
 *   arg        argument passed to fn
 *   ptid       where to write the child TID (CLONE_PARENT_SETTID)
 *   tls        TLS pointer (CLONE_SETTLS)
 *   ctid       where to clear the child TID (CLONE_CHILD_CLEARTID)
 *
 * Both return the raw syscall result:
 *   > 0   child TID, in the parent
 *     0   we are in the child
 *   < 0   -errno, in the parent
 *
 * The child-side behaviour is identical: both call fn(arg) and
 * exit with its return value via SYS_exit.
 */

/**
 * @brief Raw clone3(2) syscall wrapper.
 *
 * This is a low-level wrapper around the Linux clone3(2) syscall.
 * It does not provide any of the higher-level pthread semantics.
 *
 * The child stack pointer must be aligned to 16 bytes. The
 * function will write fn and arg at the top of the stack before
 * calling clone3(2).
 *
 * The child will call fn(arg) and exit with its return value via
 * SYS_exit. The parent can wait for the child via a futex on
 * *ctid if CLONE_CHILD_CLEARTID was used.
 */
long __haj_clone3Raw(int	(*fn)(void *),	void	*stack,	int		flags,
					  void	*arg,			int		*ptid,	void	*tls, int *ctid);

/**
 * @brief Raw clone(2) syscall wrapper.
 *
 * This is a low-level wrapper around the Linux clone(2) syscall.
 * It does not provide any of the higher-level pthread semantics.
 *
 * The child stack pointer must be aligned to 16 bytes. The
 * function will write fn and arg at the top of the stack before
 * calling clone(2).
 *
 * The child will call fn(arg) and exit with its return value via
 * SYS_exit. The parent can wait for the child via a futex on
 * *ctid if CLONE_CHILD_CLEARTID was used.
 */
long __haj_cloneLegacyRaw(int	(*fn)(void *),	void	*stack,	int		flags,
						  void	*arg,			int		*ptid,	void	*tls, int *ctid);

# endif /* HAJ_OS_LINUX */

#endif /* _BITS_THREAD_CLONE_H */
