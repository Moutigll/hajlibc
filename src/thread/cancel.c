/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file cancel.c
 * @brief Thread cancellation implementation.
 * @Created: 2026/10/05 10:27:58 by Moutig
 * @Updated: 2026/10/05 10:55:52 by Moutig
 *
 * DEFERRED cancellation is cooperative: the target checks a
 * flag at every cancellation point.
 *
 * ASYNCHRONOUS cancellation is preemptive: the target receives
 * SIGCANCEL and acts in the handler.
 *
 * pthread_cancel() sets cancelPending and sends SIGCANCEL to
 * the target via tgkill(). The handler:
 *   - returns immediately if the state is DISABLE,
 *   - terminates the thread via __haj_cancelAct() if the type
 *     is ASYNCHRONOUS,
 *   - does nothing if DEFERRED: the syscall was interrupted
 *     (EINTR) and the cancellation point checks the flag.
 *
 * SIGCANCEL is installed lazily on the first pthread_cancel().
 */

#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <bits/syscall.h>
#include <bits/thread/thread.h>

/*
 * The public cleanup frame is a char buffer. This static
 * assert guarantees it is large enough for the internal
 * layout, so __haj_cleanupPush can safely cast between them.
 */
_Static_assert(sizeof(struct __haj_thCleanup)
			   <= sizeof(__haj_cleanup_frame_t),
			   "public __haj_cleanup_frame_t is too small for "
			   "struct __haj_thCleanup");

#if defined(HAJ_OS_LINUX)
# define HAJ_SIGCANCEL	32
#else
# error "cancel.c: unsupported OS"
#endif

/* ----- Cleanup handlers ----- */

void __haj_cleanupPush(__haj_cleanup_frame_t *frame, void (*routine)(void *), void *arg)
{
	struct __haj_tcb		*tcb = __haj_tcbSelf();
	struct __haj_thCleanup	*cl = (struct __haj_thCleanup *)(void *)frame;

	cl->prev = tcb->cleanupStack;
	cl->routine = routine;
	cl->arg = arg;
	tcb->cleanupStack = cl;
}

void __haj_cleanupPop(int execute)
{
	struct __haj_tcb		*tcb = __haj_tcbSelf();
	struct __haj_thCleanup	*frame;

	frame = tcb->cleanupStack;
	if (frame == NULL)
		return;
	tcb->cleanupStack = frame->prev;
	if (execute && frame->routine != NULL)
		frame->routine(frame->arg);
}

void __haj_runCleanupHandlers(struct __haj_tcb *tcb)
{
	while (tcb->cleanupStack != NULL) {
		struct __haj_thCleanup *frame = tcb->cleanupStack;

		tcb->cleanupStack = frame->prev;
		if (frame->routine != NULL)
			frame->routine(frame->arg);
	}
}

/* ----- Cancellation state and type ----- */

int __haj_cancelPending(void)
{
	struct __haj_tcb *tcb = __haj_tcbSelf();

	if (tcb == NULL)
		return (0);
	if (tcb->cancelState != PTHREAD_CANCEL_ENABLE)
		return (0);
	if (tcb->cancelType != PTHREAD_CANCEL_DEFERRED)
		return (0);
	return (__haj_atomic_load(&tcb->cancelPending) != 0);
}

void __haj_cancelAct(void)
{
	__haj_threadExit(PTHREAD_CANCELED);
}

void __haj_cancelPoint(void)
{
	if (__haj_cancelPending())
		__haj_cancelAct();
}

/* pthread public interface */

void pthread_testcancel(void)
{
	__haj_cancelPoint();
}

int pthread_setcancelstate(int state, int *oldstate)
{
	struct __haj_tcb	*tcb = __haj_tcbSelf();
	int					old;

	if (state != PTHREAD_CANCEL_ENABLE && state != PTHREAD_CANCEL_DISABLE)
		return (EINVAL);
	if (tcb == NULL)
		return (EINVAL);

	old = tcb->cancelState;
	tcb->cancelState = state;
	if (oldstate != NULL)
		*oldstate = old;

	/*
	 * Re-enabling cancellation with a request pending means
	 * we must act on it now, per POSIX.
	 */
	if (state == PTHREAD_CANCEL_ENABLE
		&& __haj_atomic_load(&tcb->cancelPending))
		pthread_testcancel();
	return (0);
}

int pthread_setcanceltype(int type, int *oldtype)
{
	struct __haj_tcb	*tcb = __haj_tcbSelf();
	int					old;

	if (type != PTHREAD_CANCEL_DEFERRED
		&& type != PTHREAD_CANCEL_ASYNCHRONOUS)
		return (EINVAL);
	if (tcb == NULL)
		return (EINVAL);

	old = tcb->cancelType;
	tcb->cancelType = type;
	if (oldtype != NULL)
		*oldtype = old;

	if (type == PTHREAD_CANCEL_ASYNCHRONOUS
		&& __haj_atomic_load(&tcb->cancelPending)
		&& tcb->cancelState == PTHREAD_CANCEL_ENABLE)
		__haj_cancelAct();
	return (0);
}

/* ----- Signal handler ----- */

static void __haj_cancelHandler(int sig)
{
	struct __haj_tcb *tcb;

	(void)sig;
	tcb = __haj_tcbSelf();
	if (tcb == NULL)
		return;
	if (tcb->cancelState == PTHREAD_CANCEL_DISABLE)
		return;
	if (tcb->cancelType == PTHREAD_CANCEL_ASYNCHRONOUS)
		__haj_cancelAct();
	/*
	 * DEFERRED: the syscall was interrupted, the cancellation
	 * point checks the flag.
	 */
}

/* ----- Signal installation ----- */

static pthread_once_t	g_cancelOnce = PTHREAD_ONCE_INIT;

static void installCancelHandler(void)
{
	struct sigaction	sa;

	sa.sa_handler = __haj_cancelHandler;
	sa.sa_flags = 0;
	sa.sa_restorer = NULL;
	sigemptyset(&sa.sa_mask);

	sigaction(HAJ_SIGCANCEL, &sa, NULL);
}

/* ----- pthread_cancel ----- */

int pthread_cancel(pthread_t thread)
{
	struct __haj_tcb	*tcb;
	pid_t				tid;
	long				tgid;

	pthread_once(&g_cancelOnce, installCancelHandler);

	tcb = (struct __haj_tcb *)(void *)thread;
	if (tcb == NULL || tcb->state == HAJ_THREAD_EXITED)
		return (ESRCH);

	__haj_atomic_store(&tcb->cancelPending, 1);

	tid = __haj_gettid_thread(thread);
	if (tid < 0)
		return (errno);

	tgid = __haj_syscall0(SYS_getpid);
	if (tgid > 0)
		__haj_syscall3(SYS_tgkill, tgid, (long)tid,
					   (long)HAJ_SIGCANCEL);
	return (0);
}
