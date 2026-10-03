/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file mutex.c
 * @brief POSIX mutex implementation.
 * @Created: 2026/10/02 12:11:16 by Moutig
 * @Updated: 2026/10/03 12:03:44 by Moutig
 *
 * The mutex is a single futex word (the `lock` field) plus a
 * few bookkeeping fields (owner TID, count, type).
 *
 * Fast path (uncontended):
 *   lock CAS 0 -> 1 succeeds, no syscall.
 *   unlock: if count is 0, atomic store lock = 0.
 *
 * Slow path (contended):
 *   lock CAS fails, we spin a few times, then call
 *   FUTEX_WAIT on the futex word.
 *   A thread that went through the slow path always takes the
 *   lock in the CONTENDED state (0 -> 2), because it cannot know
 *   whether other waiters are still sleeping. Otherwise its
 *   unlock would not wake them (lost wakeup -> deadlock).
 *   unlock: atomic exchange lock 1 -> 0; if the previous value
 *   was 2 (contended), call FUTEX_WAKE.
 *
 * Owner tracking is what makes RECURSIVE and ERRORCHECK
 * possible, and lets us return EPERM on unlock by a non-owner.
 * For NORMAL mutexes, the owner field is still set so that
 * pthread_mutex_consistent can know who owns it, but no error
 * is checked.
 *
 * Robust mutexes are handled by the kernel through the PI futex
 * ops (FUTEX_LOCK_PI, FUTEX_UNLOCK_PI, FUTEX_TRYLOCK_PI). Each
 * thread registers a robust list with set_robust_list(2); when
 * it dies while holding a robust mutex, the kernel walks that
 * list and sets FUTEX_OWNER_DIED on the futex word, so the next
 * locker gets EOWNERDEAD.
 */

#include <pthread.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <bits/thread/thread.h>
#include <bits/thread/pthread.h>
#include <bits/syscall.h>

/*
 * The futex word has three states:
 *   0  free
 *   1  locked, no waiter
 *   2  locked, at least one waiter (so the unlocker must wake)
 */
# define HAJ_MUTEX_FREE		0
# define HAJ_MUTEX_LOCKED	1
# define HAJ_MUTEX_CONTENDED	2

/* Number of spin iterations before falling back to the futex. */
# define HAJ_MUTEX_SPIN	100

/*
 * Internal sentinel returned by the fast-path helpers when the
 * caller must continue with the slow path. It is not a valid
 * pthread error code (which are all positive).
 */
# define HAJ_MUTEX_NOT_HANDLED	(-1)


/* ----- Common helpers ----- */

/**
 * @brief Record the current thread as owner and reset the count.
 * @param m The mutex.
 * @param tid The current thread id.
 */
static __HAJ_INLINE void setOwner(struct _hajThreadMutex *m, int tid)
{
	m->owner = tid;
	m->count = 1;
}

/**
 * @brief Forget the owner of a mutex.
 * @param m The mutex.
 */
static __HAJ_INLINE void clearOwner(struct _hajThreadMutex *m)
{
	m->owner = 0;
	m->count = 0;
}

/**
 * @brief Handle RECURSIVE and ERRORCHECK reentrancy.
 *
 * Called after a failed fast-path acquire. Returns 0 when the
 * mutex was successfully re-acquired (recursive case), EDEADLK
 * when the caller already owns it (errorcheck case), or
 * HAJ_MUTEX_NOT_HANDLED when the caller must continue with the
 * slow path.
 * @param m The mutex.
 * @param tid The current thread id.
 * @return 0, EDEADLK, or HAJ_MUTEX_NOT_HANDLED.
 */
static int checkReentrant(struct _hajThreadMutex *m, int tid)
{
	if (m->type == PTHREAD_MUTEX_RECURSIVE && m->owner == tid) {
		m->count++;
		return (0);
	}
	if (m->type == PTHREAD_MUTEX_ERRORCHECK && m->owner == tid)
		return (EDEADLK);
	return (HAJ_MUTEX_NOT_HANDLED);
}

/**
 * @brief Fast-path acquisition shared by all lock variants.
 *
 * Tries the uncontended CAS, then the reentrant check. Returns
 * HAJ_MUTEX_NOT_HANDLED when the caller must continue with the
 * slow path (or, for trylock, give up with EBUSY).
 * @param m The mutex.
 * @param tid The current thread id.
 * @return 0 on success, an errno value, or HAJ_MUTEX_NOT_HANDLED.
 */
static __HAJ_INLINE int lockFastPath(struct _hajThreadMutex *m, int tid)
{
	int	expected = HAJ_MUTEX_FREE;

	if (__haj_atomic_cas(&m->lock, &expected, HAJ_MUTEX_LOCKED)) {
		setOwner(m, tid);
		return (0);
	}
	return (checkReentrant(m, tid));
}

/**
 * @brief Common unlock checks: ownership and recursion.
 *
 * Returns EPERM if a non-owner tries to unlock a RECURSIVE or
 * ERRORCHECK mutex. For a RECURSIVE mutex owned by the caller,
 * decrements the count; when it reaches zero the caller must
 * proceed with the real unlock, otherwise *done is set to 1
 * and the caller can return immediately.
 * @param m The mutex.
 * @param tid The current thread id.
 * @param done Set to 1 when the unlock is already complete.
 * @return 0 on success, EPERM on invalid owner.
 */
static int unlockPrepare(struct _hajThreadMutex *m, int tid, int *done)
{
	*done = 0;

	if ((m->type == PTHREAD_MUTEX_ERRORCHECK
		 || m->type == PTHREAD_MUTEX_RECURSIVE) && m->owner != tid)
		return (EPERM);

	if (m->type == PTHREAD_MUTEX_RECURSIVE && m->owner == tid) {
		m->count--;
		if (m->count > 0) {
			*done = 1;
			return (0);
		}
	}
	return (0);
}


/* ----- Futex primitives ----- */

/**
 * @brief Try to acquire a mutex in the CONTENDED state.
 *
 * Used by the slow path only: a thread that may have slept must
 * take the lock as CONTENDED so that its unlock wakes the other
 * sleeping waiters, if any.
 * @param m The mutex to acquire.
 * @return 1 if the mutex was acquired, 0 otherwise.
 */
static int tryAcquireContended(struct _hajThreadMutex *m)
{
	int	expected = HAJ_MUTEX_FREE;

	return (__haj_atomic_cas(&m->lock, &expected, HAJ_MUTEX_CONTENDED));
}

/**
 * @brief Wait on a mutex futex.
 *
 * This function waits on the mutex's futex word until it is
 * released or the absolute timeout is reached. If the mutex is
 * already free, it returns immediately.
 * The futex word is expected to be CONTENDED: that is the value
 * the callers have just written, and FUTEX_WAIT with any other
 * value would return EAGAIN right away (busy loop).
 * @param m The mutex to wait on.
 * @param abstime Absolute timeout, or NULL for infinite wait.
 * @return 0 on success, or an errno value on failure.
 */
static int waitFutex(struct _hajThreadMutex *m, const struct timespec *abstime)
{
	int	expected;
	int	shared;

	expected = __haj_atomic_load(&m->lock);
	if (expected == HAJ_MUTEX_FREE)
		return (0);

	shared = HAJ_MUTEX_IS_SHARED(m);

	if (abstime != NULL) {
		int r = __haj_futexWaitBitsetOp(&m->lock,
										HAJ_MUTEX_CONTENDED,
										abstime,
										FUTEX_BITSET_MATCH_ANY,
										shared);
		if (r < 0) {
			if (errno == EAGAIN || errno == EINTR)
				return (0);
			return (errno);
		}
		return (0);
	}

	if (__haj_futexWaitOp(&m->lock, HAJ_MUTEX_CONTENDED, shared) < 0) {
		if (errno == EAGAIN || errno == EINTR)
			return (0);
		return (errno);
	}
	return (0);
}


/* ----- Normal (non-robust) path ----- */

/**
 * @brief Acquire a non-robust mutex, optionally with a timeout.
 *
 * A NULL abstime means an infinite wait (pthread_mutex_lock),
 * otherwise the absolute deadline is honored
 * (pthread_mutex_timedlock).
 * @param m The mutex.
 * @param tid The current thread id.
 * @param abstime Absolute deadline, or NULL.
 * @return 0 on success, or an errno value.
 */
static int lockNormal(struct _hajThreadMutex *m, int tid, const struct timespec *abstime)
{
	int	r;

	r = lockFastPath(m, tid);
	if (r != HAJ_MUTEX_NOT_HANDLED)
		return (r);

	for (;;) {
		int v = __haj_atomic_load(&m->lock);

		if (v == HAJ_MUTEX_FREE) {
			if (tryAcquireContended(m))
				break;
			continue;
		}
		if (v == HAJ_MUTEX_LOCKED)
			(void)__haj_atomic_cas(&m->lock, &v, HAJ_MUTEX_CONTENDED);
		r = waitFutex(m, abstime);
		if (r != 0)
			return (r);
	}

	setOwner(m, tid);
	return (0);
}

/**
 * @brief Release a non-robust mutex.
 * @param m The mutex.
 * @param tid The current thread id.
 * @return 0 on success, or an errno value.
 */
static int unlockNormal(struct _hajThreadMutex *m, int tid)
{
	int	done;
	int	r;
	int	prev;

	r = unlockPrepare(m, tid, &done);
	if (r != 0)
		return (r);
	if (done)
		return (0);

	clearOwner(m);

	prev = __haj_atomic_exchange(&m->lock, HAJ_MUTEX_FREE);
	if (prev == HAJ_MUTEX_CONTENDED)
		__haj_futexWakeOp(&m->lock, 1, HAJ_MUTEX_IS_SHARED(m));

	return (0);
}


/* ----- Robust path ----- */

#if HAJ_PTHREAD_PROCESS_SHARED

# define ROBUST_PI(n)		((struct _hajRobustNode *)((uintptr_t)(n) | 1))
# define ROBUST_UNTAG(n)	((struct _hajRobustNode *)((uintptr_t)(n) & ~(uintptr_t)1))
# define ROBUST_BARRIER()	__asm__ volatile ("" ::: "memory")

/**
 * @brief Check if a mutex is robust.
 *
 * Robust mutexes use the kernel's Priority Inheritance futex
 * ops (FUTEX_LOCK_PI, FUTEX_UNLOCK_PI, FUTEX_TRYLOCK_PI). The
 * kernel encodes the owner TID and PID in the futex word, so we
 * must never CAS or exchange it ourselves.
 * @param m The mutex to check.
 * @return 1 if the mutex is robust, 0 otherwise.
 */
static __HAJ_INLINE int isRobust(struct _hajThreadMutex *m)
{
	return (m->robust == PTHREAD_MUTEX_ROBUST);
}

/**
 * @brief Initialize a thread's robust list head.
 *
 * Must be called once per execution context (main thread, each
 * new thread, and the child of fork()). The kernel resets the
 * robust list to NULL on clone() and fork(), so it has to be
 * re-registered every time.
 * @param h The robust list head to initialize.
 */
void __haj_robustInit(struct _hajRobustHead *h)
{
	h->list.next = &h->list;
	h->futexOffset = (long)offsetof(struct _hajThreadMutex, lock)
				   - (long)offsetof(struct _hajThreadMutex, robustNext);
	h->pending = NULL;
	__haj_syscall2(SYS_set_robust_list, (long)h, (long)sizeof(*h));
}

/**
 * @brief Get the robust list head.
 * @return A pointer to the robust list head.
 */
static __HAJ_INLINE struct _hajRobustHead *robustHead(void)
{
	return (&__haj_tcbSelf()->robustList);
}

/**
 * @brief Add a mutex to the caller's robust list.
 * @param h The robust list head.
 * @param m The mutex to add.
 */
static __HAJ_INLINE void robustEnqueue(struct _hajRobustHead *h,
									   struct _hajThreadMutex *m)
{
	m->robustNext.next = h->list.next;   /* garde le tag du suivant */
	ROBUST_BARRIER();
	h->list.next = ROBUST_PI(&m->robustNext);
}

/**
 * @brief Remove a mutex from the robust list.
 * @param h The robust list head.
 * @param m The mutex to remove.
 */
static void robustDequeue(struct _hajRobustHead *h, struct _hajThreadMutex *m)
{
	uintptr_t *slot = (uintptr_t *)&h->list.next;

	while (ROBUST_UNTAG((struct _hajRobustNode *)*slot) != &h->list) {
		struct _hajRobustNode *e = ROBUST_UNTAG((struct _hajRobustNode *)*slot);

		if (e == &m->robustNext) {
			*slot = (uintptr_t)e->next;
			return;
		}
		slot = (uintptr_t *)&e->next;
	}
}

/**
 * @brief Arm the pending pointer before a robust futex syscall.
 *
 * pending covers the window between the syscall and the enqueue
 * below: if the thread dies right after the kernel gave it the
 * mutex but before we linked it into the list, the kernel still
 * sees pending and marks the mutex as OWNER_DIED.
 * @param m The mutex.
 * @return The robust list head.
 */
static __HAJ_INLINE struct _hajRobustHead *robustPendingBegin(
	struct _hajThreadMutex *m)
{
	struct _hajRobustHead *h = robustHead();

	h->pending = ROBUST_PI(&m->robustNext);
	ROBUST_BARRIER();
	return (h);
}

/**
 * @brief Disarm the pending pointer after a failed syscall.
 * @param h The robust list head.
 */
static __HAJ_INLINE void robustPendingAbort(struct _hajRobustHead *h)
{
	h->pending = NULL;
}

/**
 * @brief Commit a successful robust acquisition.
 *
 * Enqueues the mutex, disarms pending, records the owner and
 * reports whether the previous owner died.
 * @param h The robust list head.
 * @param m The mutex.
 * @param tid The current thread id.
 * @return 0 on success, EOWNERDEAD if the previous owner died.
 */
static __HAJ_INLINE int robustPendingCommit(struct _hajRobustHead *h, struct _hajThreadMutex *m, int tid)
{
	robustEnqueue(h, m);
	ROBUST_BARRIER();
	h->pending = NULL;

	setOwner(m, tid);

	/*
	 * The kernel leaves FUTEX_OWNER_DIED set on the futex word
	 * when the previous owner died. There is no special return
	 * value: FUTEX_LOCK_PI returns 0, and the caller must call
	 * pthread_mutex_consistent to mark the mutex consistent.
	 */
	if (__haj_atomic_load(&m->lock) & FUTEX_OWNER_DIED)
		return (EOWNERDEAD);
	return (0);
}

/**
 * @brief Acquire a robust mutex, optionally with a timeout.
 *
 * A NULL abstime means an infinite wait (pthread_mutex_lock),
 * otherwise the absolute deadline is honored
 * (pthread_mutex_timedlock).
 * @param m The mutex.
 * @param tid The current thread id.
 * @param abstime Absolute deadline, or NULL.
 * @return 0, EOWNERDEAD, or an errno value.
 */
static int lockRobust(struct _hajThreadMutex *m, int tid, const struct timespec *abstime)
{
	struct _hajRobustHead	*h;
	int	r;

	r = checkReentrant(m, tid);
	if (r != HAJ_MUTEX_NOT_HANDLED)
		return (r);

	h = robustPendingBegin(m);

	if (abstime != NULL)
		r = __haj_futexLockPiTimedOp(&m->lock, abstime,
									 HAJ_MUTEX_IS_SHARED(m));
	else
		r = __haj_futexLockPiOp(&m->lock, HAJ_MUTEX_IS_SHARED(m));

	if (r < 0) {
		int	e = errno;

		robustPendingAbort(h);
		/*
		 * EDEADLK: this thread already owns the mutex
		 * (only possible for robust NORMAL, which POSIX
		 * defines as undefined, but glibc returns
		 * EDEADLK). EINVAL: not a robust mutex in the
		 * kernel's view. ETIMEDOUT: deadline reached.
		 */
		return (e);
	}

	return (robustPendingCommit(h, m, tid));
}

/**
 * @brief Try to acquire a robust mutex without blocking.
 * @param m The mutex.
 * @param tid The current thread id.
 * @return 0, EOWNERDEAD, EBUSY, or an errno value.
 */
static int trylockRobust(struct _hajThreadMutex *m, int tid)
{
	struct _hajRobustHead	*h;
	int	r;

	r = checkReentrant(m, tid);
	if (r != HAJ_MUTEX_NOT_HANDLED)
		return (r);

	h = robustPendingBegin(m);

	if (__haj_futexTryLockPiOp(&m->lock, HAJ_MUTEX_IS_SHARED(m)) < 0) {
		int	e = errno;

		robustPendingAbort(h);
		if (e == EAGAIN || e == EDEADLK)
			return (EBUSY);
		return (e);
	}

	return (robustPendingCommit(h, m, tid));
}

/**
 * @brief Release a robust mutex.
 * @param m The mutex.
 * @param tid The current thread id.
 * @return 0 on success, or an errno value.
 */
static int unlockRobust(struct _hajThreadMutex *m, int tid)
{
	struct _hajRobustHead	*h;
	int	done;
	int	r;

	r = unlockPrepare(m, tid, &done);
	if (r != 0)
		return (r);
	if (done)
		return (0);

	h = robustHead();

	/*
	 * Dequeue BEFORE unlocking: if we die between the dequeue
	 * and the unlock, the mutex is no longer in our list, so
	 * the kernel will not mark it OWNER_DIED even though we
	 * still hold it. That is the desired behavior: we are
	 * about to release it anyway.
	 */
	h->pending = ROBUST_PI(&m->robustNext);
	robustDequeue(h, m);
	ROBUST_BARRIER();

	/*
	 * The kernel owns the futex word. FUTEX_UNLOCK_PI clears
	 * it and wakes the next waiter; we must not touch it.
	 */
	if (__haj_futexUnlockPiOp(&m->lock, HAJ_MUTEX_IS_SHARED(m)) < 0) {
		int	e = errno;

		/* Rollback: we did not actually unlock. */
		robustEnqueue(h, m);
		h->pending = NULL;
		return (e);
	}

	h->pending = NULL;
	clearOwner(m);
	return (0);
}

#endif /* HAJ_PTHREAD_PROCESS_SHARED */


/* ----- Public API ----- */

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr)
{
	struct _hajThreadMutex	*m;

	if (mutex == NULL)
		return (EINVAL);

	m = HAJ_MUTEX(mutex);
	m->lock		= HAJ_MUTEX_FREE;
	m->owner	= 0;
	m->count	= 0;
	m->type		= PTHREAD_MUTEX_DEFAULT;
	m->robust	= PTHREAD_MUTEX_STALLED;
#if HAJ_PTHREAD_PROCESS_SHARED
	m->pshared	= PTHREAD_PROCESS_PRIVATE;
#endif

	if (attr != NULL) {
		const struct _hajThreadMutexAttr *a = HAJ_MUTEXATTR_CONST(attr);

		m->type = a->type;
		m->robust = a->robust;
#if HAJ_PTHREAD_PROCESS_SHARED
		m->pshared = a->pshared;
#endif
	}

	return (0);
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	if (mutex == NULL)
		return (EINVAL);

	/*
	 * POSIX says destroying a locked or contended mutex is
	 * undefined. We do not check, to keep the fast path fast.
	 * A well-behaved program never destroys a locked mutex.
	 */
	return (0);
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	struct _hajThreadMutex	*m;
	int	tid;

	if (mutex == NULL)
		return (EINVAL);

	m = HAJ_MUTEX(mutex);
	tid = __haj_gettid();

#if HAJ_PTHREAD_PROCESS_SHARED
	if (isRobust(m))
		return (lockRobust(m, tid, NULL));
#endif
	return (lockNormal(m, tid, NULL));
}

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	struct _hajThreadMutex	*m;
	int	r;
	int	tid;

	if (mutex == NULL)
		return (EINVAL);

	m = HAJ_MUTEX(mutex);
	tid = __haj_gettid();

#if HAJ_PTHREAD_PROCESS_SHARED
	if (isRobust(m))
		return (trylockRobust(m, tid));
#endif

	r = lockFastPath(m, tid);
	if (r != HAJ_MUTEX_NOT_HANDLED)
		return (r);
	return (EBUSY);
}

int pthread_mutex_timedlock(pthread_mutex_t *mutex,
							const struct timespec *abstime)
{
	struct _hajThreadMutex	*m;
	int	tid;

	if (mutex == NULL)
		return (EINVAL);

	m = HAJ_MUTEX(mutex);
	tid = __haj_gettid();

#if HAJ_PTHREAD_PROCESS_SHARED
	if (isRobust(m))
		return (lockRobust(m, tid, abstime));
#endif
	return (lockNormal(m, tid, abstime));
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	struct _hajThreadMutex	*m;
	int	tid;

	if (mutex == NULL)
		return (EINVAL);

	m = HAJ_MUTEX(mutex);
	tid = __haj_gettid();

#if HAJ_PTHREAD_PROCESS_SHARED
	if (isRobust(m))
		return (unlockRobust(m, tid));
#endif
	return (unlockNormal(m, tid));
}

int pthread_mutex_consistent(pthread_mutex_t *mutex)
{
	struct _hajThreadMutex	*m;

	if (mutex == NULL)
		return (EINVAL);

	m = HAJ_MUTEX(mutex);

	if (m->robust != PTHREAD_MUTEX_ROBUST)
		return (EINVAL);

	/*
	 * The kernel has already given us the mutex (the lock
	 * call that returned EOWNERDEAD succeeded). Calling
	 * consistent just tells the runtime the protected state
	 * has been repaired.
	 *
	 * On Linux, FUTEX_LOCK_PI clears the FUTEX_OWNER_DIED bit
	 * when the next lock acquires the mutex, so there is no
	 * additional kernel call to make here. We just return 0.
	 */
	return (0);
}
