/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file thread.h
 * @brief Threading internals (central header).
 * @Created: 2026/09/30 05:18:03 by Moutig
 * @Updated: 2026/10/05 11:43:47 by Moutig
 *
 * Central internal header for threading. It pulls in every
 * other internal thread header and declares the primitives that
 * <pthread.h> builds on top of. It is NOT part of the public
 * API.
 *
 * Internal layout:
 *   bits/thread/pthreadtypes.h  opaque pthread object types
 *   bits/thread/clone.h         clone(2) flags
 *   bits/thread/futex.h         futex(2) constants and wrappers
 *   bits/thread/tcb.h           Thread Control Block
 *   bits/thread/pthread_attr.h  internal layout of pthread_attr_t
 *   bits/thread/thread.h        this file: primitives
 */

#ifndef _BITS_THREAD_THREAD_H
# define _BITS_THREAD_THREAD_H

# include <bits/types.h>
# include <bits/thread/pthreadtypes.h>
# include <bits/thread/clone.h>
# include <bits/thread/futex.h>
# include <bits/thread/stack.h>
# include <bits/thread/tcb.h>

/* ----- Sentinels for pthread_t ----- */

# define HAJ_PTHREAD_NULL	((pthread_t)0)
# define HAJ_PTHREAD_SELF	((pthread_t)-1)	/* main thread */

/* ----- Atomic primitives ----- */

# if defined(__HAJ_COMPILER_GNULIKE)

#  define __haj_atomic_load(p) \
	__atomic_load_n((p), __ATOMIC_SEQ_CST)
#  define __haj_atomic_store(p, v) \
	__atomic_store_n((p), (v), __ATOMIC_SEQ_CST)
#  define __haj_atomic_cas(p, old, new) \
	__atomic_compare_exchange_n((p), (old), (new), 0, \
								__ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#  define __haj_atomic_exchange(p, v) \
	__atomic_exchange_n((p), (v), __ATOMIC_SEQ_CST)
#  define __haj_atomic_add_fetch(p, v) \
	__atomic_add_fetch((p), (v), __ATOMIC_SEQ_CST)
#  define __haj_atomic_sub_fetch(p, v) \
	__atomic_sub_fetch((p), (v), __ATOMIC_SEQ_CST)

# else
#  error "hajlib requires GCC or Clang for atomics"
# endif

/* ----- CPU relaxation ----- */

static __HAJ_INLINE void __haj_cpuRelax(void)
{
# if defined(HAJ_ARCH_X86_64) || defined(HAJ_ARCH_I386)
	__asm__ volatile ("pause" ::: "memory");
# elif defined(HAJ_ARCH_AARCH64) || defined(HAJ_ARCH_ARM)
	__asm__ volatile ("yield" ::: "memory");
# else
	/* Nothing. */
# endif
}

/* ----- Kernel TID ----- */

/**
 * @brief Get the kernel thread ID of the calling thread.
 *
 * This is a thin wrapper around the gettid(2) syscall. It is
 * used to implement pthread_self() and for futexes.
 */
int __haj_gettid(void);

/**
 * @brief Get the kernel thread ID of a specific pthread_t.
 *
 * This is a thin wrapper around __haj_gettid() and the TCB.
 * It returns the kernel TID of the thread represented by
 * the given pthread_t. If the pthread_t is invalid, it
 * returns -1.
 *
 * @param thread The pthread_t of the thread to query.
 * @return The kernel TID of the thread, or -1 if invalid.
 */
pid_t __haj_gettid_thread(pthread_t thread);
/* ----- Thread creation ----- */

/**
 * @brief Get the TCB of the calling thread.
 *
 * This is a thin wrapper around __haj_tcbSelf(). It is used to
 * implement pthread_self() and for futexes.
 */
struct __haj_tcb *__haj_tcbCreate(void *stackBase, size_t stackSize,
								  size_t guardSize, void *stackTop,
								  void **childStack);

/**
 * @brief Run all TSD destructors for a thread.
 *
 * Called by __haj_threadExit. Iterates over all keys with a
 * destructor, calls it with the thread's non-NULL value, and
 * clears the slot. Up to HAJ_PTHREAD_DESTRUCTOR_ITERATIONS
 * passes are made, in case a destructor re-sets a value.
 *
 * @param tcb The TCB of the exiting thread.
 */
void __haj_runTlsDestructors(struct __haj_tcb *tcb);

/**
 * @brief Free a TCB and its associated stack.
 *
 * Unmaps the entire region: guard page, stack, TLS image, TCB.
 * @param tcb Pointer to the TCB of the thread to free.
 */
void __haj_threadExit(void *retval) __HAJ_NORETURN;

/* ----- Global thread list ----- */

/**
 * @brief Add a TCB to the global thread list.
 *
 * The global thread list is a singly-linked list of all
 * threads in the process. It is used for cleanup and for
 * iterating over threads. This function adds a TCB to the
 * head of the list.
 *
 * @param tcb Pointer to the TCB of the thread to add.
 */
void __haj_threadListAdd(struct __haj_tcb *tcb);

/**
 * @brief Remove a TCB from the global thread list.
 *
 * This function removes a TCB from the global thread list.
 * It is called when a thread exits and its TCB is freed.
 *
 * @param tcb Pointer to the TCB of the thread to remove.
 */
void __haj_threadListRemove(struct __haj_tcb *tcb);

/* ----- Thread entry point ----- */

/**
 * @brief Start a new thread.
 *
 * This function is called by the new thread after clone(2) returns.
 * It sets up the TCB, calls the thread's start routine, and handles
 * cleanup when the thread exits.
 *
 * @param tcb Pointer to the TCB of the new thread.
 */
void __haj_threadStart(struct __haj_tcb *tcb) __HAJ_NORETURN;

/**
 * @brief Trampoline for the thread entry point.
 *
 * This function is used as a trampoline to call __haj_threadStart
 * with the correct argument type. It is called by the clone wrapper
 * after the new thread is created.
 *
 * @param arg Pointer to the TCB of the new thread.
 * @return The return value of __haj_threadStart (never returns).
 */
int __haj_threadTrampoline(void *arg);

/**
 * @brief Reset the global thread list.
 *
 * This function resets the global thread list to its initial state.
 * It is called during initialization and when the process is reset.
 *
 * @param self Pointer to the TCB of the current thread.
 */
static __HAJ_INLINE void __haj_threadListReset(struct __haj_tcb *self)
{
	__haj_threadListLock = 0;
	self->next = self;
	self->prev = self;
	__haj_threadList = self;
}

/* ----- Cancellation (internal) ----- */

/**
 * @brief Check if cancellation is pending for the current thread.
 *
 * This function checks if cancellation is pending for the calling
 * thread. It returns 1 if cancellation is pending, and 0 otherwise.
 *
 * @return 1 if cancellation is pending, 0 otherwise.
 */
int		__haj_cancelPending(void);

/**
 * @brief Act on a pending cancellation request.
 *
 * This function is called when a thread has a pending cancellation
 * request and is in a cancellation point. It performs the necessary
 * cleanup and terminates the thread with PTHREAD_CANCELED.
 */
void	__haj_cancelAct(void) __HAJ_NORETURN;

/**
 * @brief Check for cancellation at a cancellation point.
 *
 * This function is called at cancellation points to check if
 * cancellation is pending. If it is, it calls __haj_cancelAct().
 */
void	__haj_cancelPoint(void);

/**
 * @brief Run the cleanup handlers for a thread.
 *
 * This function runs all the cleanup handlers for the specified thread,
 * in the reverse order of their registration.
 *
 * @param tcb Pointer to the TCB of the thread whose cleanup handlers
 *            need to be run.
 */
void	__haj_runCleanupHandlers(struct __haj_tcb *tcb);

/* ----- At fork ----- */

# ifndef HAJ_ATFORK_MAX
#  define HAJ_ATFORK_MAX	64
# endif

/**
 * @brief An entry in the atfork handler list.
 *
 * This structure represents a single atfork handler, containing
 * pointers to the prepare, parent, and child functions.
 */
struct _hajAtforkEntry {
	void	(*prepare)(void);
	void	(*parent)(void);
	void	(*child)(void);
};

/**
 * @brief Call the atfork prepare handlers.
 *
 * This function is called before a fork() system call to invoke all
 * registered atfork prepare handlers.
 */
void __haj_atforkPrepare(void);

/**
 * @brief Call the atfork parent handlers.
 *
 * This function is called in the parent process after a fork() system
 * call to invoke all registered atfork parent handlers.
 */
void __haj_atforkParent(void);

/**
 * @brief Call the atfork child handlers.
 *
 * This function is called in the child process after a fork() system
 * call to invoke all registered atfork child handlers.
 */
void __haj_atforkChild(void);

/* ----- Cleanup handlers ----- */

/**
 * @brief A cleanup handler for a thread.
 *
 * This structure represents a single cleanup handler in the thread's
 * cleanup stack. It contains a pointer to the previous handler, a
 * function to call, and an argument to pass to that function.
 */
struct __haj_thCleanup {
	struct __haj_thCleanup	*prev;
	void					(*routine)(void *);
	void					*arg;
};

#endif /* _BITS_THREAD_THREAD_H */
