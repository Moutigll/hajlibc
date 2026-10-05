/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file signal.h
 * @brief Signal numbers and constants.
 * @Created: 2026/09/30 11:27:57 by Moutig
 * @Updated: 2026/10/03 16:06:47 by Moutig
 *
 * Defines the POSIX signal numbers with the values used by the
 * target OS. The values are part of the kernel ABI: Linux,
 * FreeBSD, Darwin and Windows all use different numbering, and
 * mixing them up would send the wrong signal.
 *
 * Organization:
 *   - The 3 Unix (Linux, FreeBSD, Darwin) share a common base:
 *     signals that have the same number on all 3 are defined
 *     once, before the per-OS #if.
 *   - Signals that differ per OS are defined inside the #if.
 *   - Windows is fully separate: its CRT only provides a small
 *     subset with different values.
 *
 * Do NOT include this header directly from user code. Use
 * <signal.h> instead.
 *
 * References:
 *   Linux:   <asm/signal.h>, <asm-generic/signal.h>
 *   FreeBSD: <sys/signal.h>
 *   Darwin:  <sys/signal.h>
 *   Windows: <signal.h> from the mingw CRT
 */

#ifndef _BITS_SIGNAL_H
# define _BITS_SIGNAL_H

# include <bits/os.h>
#include <stddef.h>

/*
 * On Linux, sigset_t is an array of unsigned long, with enough
 * bits for 64 signals (NSIG = 64). The kernel expects exactly
 * 8 bytes for rt_sigaction/rt_sigprocmask on 64-bit, and 4 bytes
 * on 32-bit. We always use 8 bytes so the ABI is stable.
 *
 * POSIX does not specify the layout; user code must use the
 * sigemptyset / sigaddset / ... functions.
 */
typedef struct {
	unsigned long	__bits[1];
} sigset_t;

/*
 * Linux kernel layout for rt_sigaction(2). The sa_mask is
 * transmitted to the kernel as an 8-byte word; the user-mode
 * sigset_t is larger than that, so we expose only the low word
 * in the struct and let the wrapper zero-extend.
 *
 * The sa_restorer is required on x86_64: the kernel refuses to
 * install a handler without it if SA_RESTORER is set.
 */
struct sigaction {
	union {
		void	(*sa_handler)(int);
		void	(*sa_sigaction)(int, void *, void *);
	} __sa_handler;
	unsigned long	sa_flags;
	void			(*sa_restorer)(void);
	sigset_t		sa_mask;
};


# define sa_handler	__sa_handler.sa_handler
# define sa_sigaction	__sa_handler.sa_sigaction

/**
 * @brief Signal value union.
 *
 * Used by sigqueue(3) and sigaction(2) to pass an integer or pointer value with a signal.
 */
union sigval {
	int		sival_int;
	void	*sival_ptr;
};

/**
 * @brief Signal information structure.
 *
 * Used by sigaction(2) and sigqueue(3) to provide detailed information about a signal.
 */
typedef struct {
	int	si_signo;
	int	si_errno;
	int	si_code;
	int	__pad0;
	union {
		int	__pad[28];
		struct { int si_pid; unsigned int si_uid; }					__kill;
		struct { void *si_addr; }									__fault;
		struct { int si_status; int si_utime; int si_stime; }		__child;
		struct { union sigval si_value; int si_int; void *si_ptr; }	__rt;
	} __data;
# define si_pid		__data.__kill.si_pid
# define si_uid		__data.__kill.si_uid
# define si_addr	__data.__fault.si_addr
# define si_status	__data.__child.si_status
# define si_value	__data.__rt.si_value
# define si_int		__data.__rt.si_int
# define si_ptr		__data.__rt.si_ptr
} siginfo_t;

/*
 * stack_t describes an alternate signal stack. The layout
 * matches the kernel's stack_t (which is what sigaltstack(2)
 * expects).
 */
typedef struct {
	void	*ss_sp;
	int		ss_flags;
	size_t	ss_size;
} stack_t;

# define SIGEV_SIGNAL	0
# define SIGEV_NONE		1
# define SIGEV_THREAD	2

/*
 * sigevent_t describes how a thread should be notified of an
 * event. It is used by timer_create(2), mq_notify(3), and
 * sigqueue(3).
 */
typedef struct sigevent {
	union sigval	sigev_value;
	int				sigev_notify;
	int				sigev_signo;
	void			(*sigev_notify_function)(union sigval);
	void			*sigev_notify_attributes;
} sigevent_t;

# if defined(HAJ_ARCH_X86_64)
/**
 * @brief Machine context structure for x86_64 architecture.
 *
 * This structure describes the CPU registers and state for a thread.
 * It is used by getcontext(2), setcontext(2), and swapcontext(2).
 */
typedef struct {
	unsigned long	cr2;
	unsigned long	oldmask;
	unsigned long	r8, r9, r10, r11, r12, r13, r14, r15;
	unsigned long	rdi, rsi, rbp, rbx, rdx, rax, rcx, rsp, rip;
	unsigned long	eflags, cs, gs, fs, ss;
	unsigned long	err, trapno;
	unsigned long	fpstate;
	unsigned long	reserved[8];
} mcontext_t;

# elif defined(HAJ_ARCH_AARCH64)
/**
 * @brief Machine context structure for AArch64 architecture.
 *
 * This structure describes the CPU registers and state for a thread.
 * It is used by getcontext(2), setcontext(2), and swapcontext(2).
 */
typedef struct {
	unsigned long	__regs[31];
	unsigned long	sp;
	unsigned long	pc;
	unsigned long	pstate;
} mcontext_t;

#endif /* HAJ_ARCH_AARCH64 */

/*
 * ucontext_t describes the user context of a thread. It is used
 * by getcontext(2), setcontext(2), and swapcontext(2).
 */
typedef struct ucontext_t {
	unsigned long		uc_flags;
	struct ucontext_t	*uc_link;
	stack_t				uc_stack;
	mcontext_t			uc_mcontext;
	sigset_t			uc_sigmask;
} ucontext_t;

typedef int sig_atomic_t;

/* ----- Signal handler special values ----- */

# ifndef SIG_DFL
#  define SIG_DFL	((void (*)(int))0)
# endif
# ifndef SIG_IGN
#  define SIG_IGN	((void (*)(int))1)
# endif
# ifndef SIG_ERR
#  define SIG_ERR	((void (*)(int))-1)
# endif

# define HAJ_NSIG	64

/* ----- Windows (separate model) ----- */
/**
 * Windows has no POSIX signal model. The CRT provides a small
 * ANSI C subset with its own values. We define only these,
 * because code using POSIX-only signals cannot work on Windows
 * anyway.
 *
 * Note: SIGABRT is 22 on Windows, 6 on Unix. Code that hardcodes
 * 6 will silently send SIGBREAK instead of SIGABRT.
 */

# if defined(HAJ_OS_WINDOWS)

#  define SIGINT	2	/* Ctrl+C. */
#  define SIGILL	4	/* Illegal instruction. */
#  define SIGABRT	22	/* Abort (Windows value!). */
#  define SIGFPE	8	/* Floating-point exception. */
#  define SIGSEGV	11	/* Invalid memory reference. */
#  define SIGTERM	15	/* Termination request. */
#  define SIGBREAK	21	/* Ctrl+Break (Windows only). */

#  define SIG_DFL	((void (*)(int))0)
#  define SIG_IGN	((void (*)(int))1)
#  define SIG_ERR	((void (*)(int))-1)

# elif defined(HAJ_OS_LINUX) || defined(HAJ_OS_FREEBSD) || defined(HAJ_OS_DARWIN)

/* ----- Signals common to Linux, FreeBSD and Darwin ----- */

#  define SIGHUP	1	/* Hangup. */
#  define SIGINT	2	/* Interrupt from keyboard. */
#  define SIGQUIT	3	/* Quit from keyboard. */
#  define SIGILL	4	/* Illegal instruction. */
#  define SIGTRAP	5	/* Trace/breakpoint trap. */
#  define SIGABRT	6	/* Abort. */
#  define SIGIOT	SIGABRT	/* Historical alias. */
#  define SIGFPE	8	/* Floating-point exception. */
#  define SIGKILL	9	/* Kill (cannot be caught). */
#  define SIGPIPE	13	/* Broken pipe. */
#  define SIGALRM	14	/* Alarm clock. */
#  define SIGTERM	15	/* Termination. */
#  if defined(HAJ_OS_LINUX)
#   define SIGSTKFLT	16	/* Stack fault (Linux only). */
#endif
#  define SIGXCPU	24	/* CPU time limit exceeded. */
#  define SIGXFSZ	25	/* File size limit exceeded. */
#  define SIGVTALRM	26	/* Virtual timer expired. */
#  define SIGPROF	27	/* Profiling timer expired. */
#  define SIGWINCH	28	/* Window size change. */

/* ----- Signals that differ between Linux and the BSDs ----- */

#  if defined(HAJ_OS_LINUX)

#   define SIGBUS	7	/* Bus error. */
#   define SIGUSR1	10	/* User-defined signal 1. */
#   define SIGSEGV	11	/* Invalid memory reference. */
#   define SIGUSR2	12	/* User-defined signal 2. */
#   define SIGSTKFLT16	/* Stack fault (Linux only). */
#   define SIGCHLD	17	/* Child status changed. */
#   define SIGCLD	SIGCHLD	/* Historical alias. */
#   define SIGCONT	18	/* Continue after stop. */
#   define SIGSTOP	19	/* Stop (cannot be caught). */
#   define SIGTSTP	20	/* Stop from keyboard. */
#   define SIGTTIN	21	/* Background read from tty. */
#   define SIGTTOU	22	/* Background write to tty. */
#   define SIGURG	23	/* Urgent condition on socket. */
#   define SIGIO	29	/* I/O now possible. */
#   define SIGPOLL	SIGIO	/* Historical alias. */
#   define SIGPWR	30	/* Power failure (Linux only). */
#   define SIGSYS	31	/* Bad system call. */

/* Real-time signals (Linux-specific). */
#   define SIGRTMIN	32
#   define SIGRTMAX	64

/* Linux: sigprocmask commands. */
#   define SIG_BLOCK	0
#   define SIG_UNBLOCK	1
#   define SIG_SETMASK	2

/* Linux: sigaction flags (sa_flags). */
#   define SA_NOCLDSTOP	0x00000001
#   define SA_NOCLDWAIT	0x00000002
#   define SA_SIGINFO	0x00000004
#   define SA_ONSTACK	0x08000000
#   define SA_RESTART	0x10000000
#   define SA_NODEFER	0x40000000
#   define SA_RESETHAND	0x80000000
#   define SA_RESTORER	0x04000000	/* Internal to glibc. */

/* Linux: sigev_notify values. */
#   define SIGEV_SIGNAL		0
#   define SIGEV_NONE		1
#   define SIGEV_THREAD		2
#   define SIGEV_THREAD_ID	4	/* Linux-specific. */

/* Linux: si_code values for user-originated signals.
 * These are negative or zero, unlike the BSDs. */
#   define SI_USER		0
#   define SI_QUEUE		(-1)
#   define SI_TIMER		(-2)
#   define SI_ASYNCIO	(-4)
#   define SI_MESGQ		(-3)

/* Linux: sigaltstack flags (ss_flags). */
#   define SS_ONSTACK	1
#   define SS_DISABLE	2

/* Linux: minimum and recommended stack size for signal handlers. */
#   define MINSIGSTKSZ	2048
#   define SIGSTKSZ		8192

#  elif defined(HAJ_OS_FREEBSD) || defined(HAJ_OS_DARWIN)

/*
 * FreeBSD and Darwin share the same signal numbering for all
 * standard signals. Only the sa_flags differ (Darwin has its own
 * bit layout inherited from 4.4BSD), so they are defined
 * separately below.
 */

#   define SIGEMT	7	/* Emulator trap. */
#   define SIGBUS	10	/* Bus error. */
#   define SIGSEGV	11	/* Invalid memory reference. */
#   define SIGSYS	12	/* Bad system call. */
#   define SIGURG	16	/* Urgent condition on socket. */
#   define SIGSTOP	17	/* Stop (cannot be caught). */
#   define SIGTSTP	18	/* Stop from keyboard. */
#   define SIGCONT	19	/* Continue after stop. */
#   define SIGCHLD	20	/* Child status changed. */
#   define SIGCLD	SIGCHLD	/* Historical alias. */
#   define SIGTTIN	21	/* Background read from tty. */
#   define SIGTTOU	22	/* Background write to tty. */
#   define SIGIO	23	/* I/O now possible. */
#   define SIGPOLL	SIGIO	/* Historical alias. */
#   define SIGINFO	29	/* Status request. */
#   define SIGUSR1	30	/* User-defined signal 1. */
#   define SIGUSR2	31	/* User-defined signal 2. */

/* FreeBSD: sigprocmask commands. */
#   define SIG_BLOCK	1
#   define SIG_UNBLOCK	2
#   define SIG_SETMASK	3

/* FreeBSD/Darwin: sigev_notify values. */
#   define SIGEV_SIGNAL	1
#   define SIGEV_NONE	0
#   define SIGEV_THREAD	2

/* FreeBSD/Darwin: si_code values for user-originated signals.
 * These are large positive values, unlike Linux. */
#   define SI_USER		0x10001
#   define SI_QUEUE		0x10002
#   define SI_TIMER		0x10003
#   define SI_ASYNCIO	0x10004
#   define SI_MESGQ		0x10005

/* FreeBSD/Darwin: sigaltstack flags (ss_flags). */
#   define SS_ONSTACK	1
#   define SS_DISABLE	4

/* FreeBSD/Darwin: minimum and recommended stack size for signal handlers. */
#   define MINSIGSTKSZ	1024
#   define SIGSTKSZ		16384

#  endif	/* Linux vs BSD */

/* ----- FreeBSD-specific real-time signals ----- */

#  if defined(HAJ_OS_FREEBSD)
#   define SIGRTMIN	65
#   define SIGRTMAX	126
#  endif

/* ----- FreeBSD/Darwin: sigaction flags ----- */

#  if defined(HAJ_OS_FREEBSD)
#   define SA_NOCLDSTOP	0x00000001
#   define SA_NOCLDWAIT	0x00000002
#   define SA_SIGINFO	0x00000040
#   define SA_ONSTACK	0x00000080
#   define SA_RESTART	0x00000010
#   define SA_NODEFER	0x00000020
#   define SA_RESETHAND	0x00000004
#  endif

#  if defined(HAJ_OS_DARWIN)
#   define SA_NOCLDSTOP	0x00000008
#   define SA_NOCLDWAIT	0x00000020
#   define SA_SIGINFO	0x00000040
#   define SA_ONSTACK	0x00000001
#   define SA_RESTART	0x00000010
#   define SA_NODEFER	0x00000002
#   define SA_RESETHAND	0x00000004
#  endif

/* ----- Common si_code values for kernel-originated signals ----- */
/**
 * When a signal is generated by the kernel (hardware fault,
 * breakpoint, child status change), siginfo_t.si_code is set to
 * one of these values. The values are the same on Linux, FreeBSD
 * and Darwin.
 *
 * Note: siginfo_t itself is defined in <bits/siginfo.h>, and is
 * ABI-specific. Only the si_code values are portable.
 */

/* ----- SIGILL: illegal instruction ----- */

#  define ILL_ILLOPC	1	/* Illegal opcode. */
#  define ILL_ILLOPN	2	/* Illegal operand. */
#  define ILL_ILLADR	3	/* Illegal addressing mode. */
#  define ILL_ILLTRP	4	/* Illegal trap. */
#  define ILL_PRVOPC	5	/* Privileged opcode. */
#  define ILL_PRVREG	6	/* Privileged register. */
#  define ILL_COPROC	7	/* Coprocessor error. */
#  define ILL_BADSTK	8	/* Internal stack error. */

/* ----- SIGFPE: arithmetic exception ----- */

#  define FPE_INTDIV	1	/* Integer divide by zero. */
#  define FPE_INTOVF	2	/* Integer overflow. */
#  define FPE_FLTDIV	3	/* Floating-point divide by zero. */
#  define FPE_FLTOVF	4	/* Floating-point overflow. */
#  define FPE_FLTUND	5	/* Floating-point underflow. */
#  define FPE_FLTRES	6	/* Floating-point inexact result. */
#  define FPE_FLTINV	7	/* Invalid floating-point operation. */
#  define FPE_FLTSUB	8	/* Subscript out of range. */

/* ----- SIGSEGV: invalid memory reference ----- */

#  define SEGV_MAPERR	1	/* Address not mapped to object. */
#  define SEGV_ACCERR	2	/* Invalid permissions for mapped object. */

/* ----- SIGBUS: bus error ----- */

#  define BUS_ADRALN	1	/* Invalid address alignment. */
#  define BUS_ADRERR	2	/* Nonexistent physical address. */
#  define BUS_OBJERR	3	/* Object-specific hardware error. */

/* ----- SIGTRAP: trace/breakpoint trap ----- */

#  define TRAP_BRKPT	1	/* Process breakpoint. */
#  define TRAP_TRACE	2	/* Process trace trap. */

/* ----- SIGCHLD: child status ----- */

#  define CLD_EXITED	1	/* Child has exited. */
#  define CLD_KILLED	2	/* Child was killed. */
#  define CLD_DUMPED	3	/* Child terminated abnormally. */
#  define CLD_TRAPPED	4	/* Traced child has trapped. */
#  define CLD_STOPPED	5	/* Child has stopped. */
#  define CLD_CONTINUED	6	/* Stopped child has continued. */

/* ----- sig2str / str2sig buffer size ----- */

#  define SIG2STR_MAX	32	/* Max length of a signal name, including NUL. */

/* ----- Unix: special handler values ----- */

#  define SIG_DFL	((void (*)(int))0)	/* Default action. */
#  define SIG_IGN	((void (*)(int))1)	/* Ignore the signal. */
#  define SIG_ERR	((void (*)(int))-1)	/* Error from signal(). */

# else

#  error "hajlibc: no signal definitions for this OS"

# endif

# if defined(__x86_64__)
/**
 * @brief Trampoline for returning from a signal handler.
 *
 * This function is used as the sa_restorer for signal handlers
 * that do not provide their own. It performs the necessary
 * cleanup and returns to the interrupted context.
 */
extern void __haj_sigreturn_trampoline(void) __HAJ_NORETURN;
# endif

#endif /* _BITS_SIGNAL_H */
