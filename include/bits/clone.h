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
 * @Updated: 2026/09/30 11:29:07 by Moutig
 *
 * The clone(2) syscall is Linux-specific. It creates a new
 * process, optionally sharing different parts of the execution
 * context with the parent (memory, file descriptors, signal
 * handlers, ...). POSIX threads are created by calling clone()
 * with the right set of flags.
 *
 * The flags and their values are part of the Linux kernel ABI
 * and are stable. They come from <linux/sched.h>.
 *
 * FreeBSD and Darwin do not have clone(). FreeBSD uses
 * thr_new(2), Darwin uses bsdthread_create(2). The flags in
 * this header are Linux-only and are guarded accordingly.
 */

#ifndef _BITS_CLONE_H
# define _BITS_CLONE_H

# include <bits/os.h>
# include <bits/signal.h>

# if defined(HAJ_OS_LINUX)

/* ----- clone() flags ----- */

/**
 * The low byte of the flags argument is the signal sent to the
 * parent when the child exits. SIGCHLD is the usual value.
 *
 * The rest of the bits select what is shared between parent and
 * child. POSIX threads share almost everything.
 */

#  define CLONE_NEWTIME			0x00000080	/* New time namespace. */
#  define CLONE_VM				0x00000100	/* Share address space. */
#  define CLONE_FS				0x00000200	/* Share filesystem info (umask, cwd, root). */
#  define CLONE_FILES			0x00000400	/* Share file descriptor table. */
#  define CLONE_SIGHAND			0x00000800	/* Share signal handlers. */
#  define CLONE_PIDFD			0x00001000	/* parent_tid is a pidfd. */
#  define CLONE_PTRACE			0x00002000	/* Let parent trace child. */
#  define CLONE_VFORK			0x00004000	/* Parent waits for child exec/exit. */
#  define CLONE_PARENT			0x00008000	/* Child's parent is parent's parent. */
#  define CLONE_THREAD			0x00010000	/* Same thread group (POSIX threads). */
#  define CLONE_NEWNS			0x00020000	/* New mount namespace. */
#  define CLONE_SYSVSEM			0x00040000	/* Share SysV sem undo values. */
#  define CLONE_SETTLS			0x00080000	/* Set TLS on child. */
#  define CLONE_PARENT_SETTID	0x00100000	/* Store child TID in parent_tid. */
#  define CLONE_CHILD_CLEARTID	0x00200000	/* Clear child_tid on exit. */
#  define CLONE_DETACHED		0x00400000	/* (obsolete) */
#  define CLONE_UNTRACED		0x00800000	/* Prevent CLONE_PTRACE. */
#  define CLONE_CHILD_SETTID	0x01000000	/* Store child TID in child_tid. */
#  define CLONE_NEWCGROUP		0x02000000	/* New cgroup namespace. */
#  define CLONE_NEWUTS			0x04000000	/* New UTS namespace. */
#  define CLONE_NEWIPC			0x08000000	/* New IPC namespace. */
#  define CLONE_NEWUSER			0x10000000	/* New user namespace. */
#  define CLONE_NEWPID			0x20000000	/* New PID namespace. */
#  define CLONE_NEWNET			0x40000000	/* New network namespace. */
#  define CLONE_IO				0x80000000	/* Share I/O context. */

/* ----- The set of flags that POSIX threads use ----- */
/**
 * CLONE_VM             : share memory (threads share the address space)
 * CLONE_FS             : share cwd, root, umask
 * CLONE_FILES          : share the fd table
 * CLONE_SIGHAND        : share signal handlers
 * CLONE_THREAD         : put the child in the same thread group
 * CLONE_SYSVSEM        : share SysV sem undo values
 * CLONE_SETTLS         : set the child TLS (fs / tpidr_el0)
 * CLONE_PARENT_SETTID  : write the child TID into parent_tid
 * CLONE_CHILD_CLEARTID : clear child_tid on exit and futex-wake
 * SIGCHLD              : signal sent to the parent on child exit
 *
 * The last flag is essential: it tells the kernel to clear the
 * child_tid field when the child exits and wake anyone waiting
 * on it (futex). This is how pthread_join() is implemented.
 */

#  define HAJ_CLONE_THREAD_FLAGS \
	(CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | \
	 CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS | \
	 CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID | SIGCHLD)

/* ----- clone3() ----- */
/**
 * clone3(2) is the modern version of clone(2). It takes a struct
 * clone_args and returns a PID file descriptor. More flexible,
 * but not available on older kernels (< 5.3).
 *
 * hajlib uses clone(2) for now.
 */

struct hajCloneArgs {
	unsigned long long flags;
	unsigned long long pidfd;
	unsigned long long childTid;
	unsigned long long parentTid;
	unsigned long long exitSignal;
	unsigned long long stack;
	unsigned long long stackSize;
	unsigned long long tls;
	unsigned long long setTid;
	unsigned long long setTidSize;
	unsigned long long cgroup;
};

# endif /* HAJ_OS_LINUX */

#endif /* _BITS_CLONE_H */
