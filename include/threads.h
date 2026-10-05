/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file threads.h
 * @brief C11 threads.h compatibility header.
 * @Created: 2026/10/05 11:23:44 by Moutig
 * @Updated: 2026/10/05 12:40:00 by Moutig
 *
 * This header provides a compatibility layer for the C11 <threads.h> API
 * on top of the POSIX pthreads API. It defines the types, constants, and
 * functions specified by the C11 standard, mapping them to the corresponding
 * pthreads functions and types. This allows C11 threads code to be compiled
 * and run on systems that support pthreads, providing a consistent threading
 * interface across different platforms.
 */

#ifndef _THREADS_H
# define _THREADS_H

# include <time.h>
# include <stddef.h>
# include <pthread.h>

# ifdef __cplusplus
extern "C" {
# endif

/* ----- Types ----- */

/**
 * @brief Thread identifier type.
 *
 * This type represents a thread identifier in the C11 threads API.
 * It is an opaque type that can be used to refer to threads created
 * using thrd_create() and other thread management functions.
 */
typedef pthread_t	thrd_t;

/**
 * @brief Once flag type.
 *
 * This type represents a once flag in the C11 threads API.
 * It is an opaque type that can be used to ensure that a function is called only once.
 */
typedef pthread_once_t	once_flag;

/**
 * @brief Mutex type.
 *
 * This type represents a mutex in the C11 threads API.
 * It is an opaque type that can be used to synchronize access to shared resources.
 */
typedef pthread_mutex_t	mtx_t;

/**
 * @brief Condition variable type.
 *
 * This type represents a condition variable in the C11 threads API.
 * It is an opaque type that can be used to synchronize threads based on certain conditions.
 */
typedef pthread_cond_t	cnd_t;

/**
 * @brief Thread-specific storage key type.
 *
 * This type represents a thread-specific storage key in the C11 threads API.
 * It is an opaque type that can be used to store and retrieve data specific to a thread.
 */
typedef pthread_key_t	tss_t;

/* ----- Return codes ----- */

# define thrd_success	0
# define thrd_busy		1
# define thrd_error		2
# define thrd_nomem		3
# define thrd_timedout	4

# define ONCE_FLAG_INIT	0

# ifndef __cplusplus
#  ifndef thread_local
#   define thread_local _Thread_local
#  endif
# endif

/*
 * Number of passes the implementation makes through the
 * TSS destructors when a thread exits. A destructor that
 * sets a new non-NULL value for a key causes another pass.
 * This is the C11 equivalent of PTHREAD_DESTRUCTOR_ITERATIONS.
 */
# ifndef TSS_DTOR_ITERATIONS
#  define TSS_DTOR_ITERATIONS PTHREAD_DESTRUCTOR_ITERATIONS
# endif

/* ----- Threads ----- */

/**
 * @brief Thread start function type.
 *
 * This type represents the signature of a thread start function in the C11 threads API.
 * A thread start function takes a single void pointer argument and returns an int.
 */
typedef int (*thrd_start_t)(void *);

/**
 * @brief Create a new thread.
 *
 * This function creates a new thread that starts executing the specified function.
 *
 * @param thr Pointer to the thread identifier to be initialized.
 * @param func The function to be executed in the new thread.
 * @param arg The argument to be passed to the function.
 * @return thrd_success on success, or an error code on failure.
 */
int		thrd_create(thrd_t *thr, thrd_start_t func, void *arg);

/**
 * @brief Get the identifier of the current thread.
 *
 * This function returns the identifier of the calling thread.
 *
 * @return The identifier of the current thread.
 */
thrd_t	thrd_current(void);

/**
 * @brief Compare two thread identifiers for equality.
 *
 * This function compares two thread identifiers and returns a non-zero value if they are equal.
 *
 * @param lhs The first thread identifier to compare.
 * @param rhs The second thread identifier to compare.
 * @return Non-zero if the thread identifiers are equal, 0 otherwise.
 */
int		thrd_equal(thrd_t lhs, thrd_t rhs);

/**
 * @brief Put the current thread to sleep.
 *
 * This function puts the current thread to sleep for the specified duration.
 *
 * @param duration The duration to sleep.
 * @param remaining If not NULL, the remaining time will be stored here.
 * @return 0 on success, or an error code on failure.
 */
int		thrd_sleep(const struct timespec *duration, struct timespec *remaining);

/**
 * @brief Yield the processor to another thread.
 *
 * This function yields the processor, allowing other threads to run.
 */
void	thrd_yield(void);

/**
 * @brief Detach a thread.
 *
 * This function detaches the specified thread, allowing it to run independently.
 *
 * @param thr The thread to detach.
 * @return thrd_success on success, or an error code on failure.
 */
int		thrd_detach(thrd_t thr);

/**
 * @brief Wait for a thread to terminate.
 *
 * This function waits for the specified thread to terminate and retrieves its return value.
 *
 * @param thr The thread to join.
 * @param res If not NULL, the return value of the thread will be stored here.
 * @return thrd_success on success, or an error code on failure.
 */
int		thrd_join(thrd_t thr, int *res);

/**
 * @brief Terminate the current thread.
 *
 * This function terminates the calling thread and returns the specified exit status.
 *
 * @param res The exit status to return.
 */
_Noreturn void thrd_exit(int res);

/* ----- Mutexes ----- */

# define mtx_plain		0
# define mtx_recursive	1
# define mtx_timed		2

/**
 * @brief Initialize a mutex.
 *
 * This function initializes a mutex with the specified type.
 *
 * @param mtx The mutex to initialize.
 * @param type The type of the mutex.
 * @return 0 on success, or an error code on failure.
 */
int		mtx_init(mtx_t *mtx, int type);

/**
 * @brief Lock a mutex.
 *
 * This function locks the specified mutex, blocking if necessary.
 *
 * @param mtx The mutex to lock.
 * @return 0 on success, or an error code on failure.
 */
int		mtx_lock(mtx_t *mtx);

/**
 * @brief Try to lock a mutex.
 *
 * This function attempts to lock the specified mutex without blocking.
 *
 * @param mtx The mutex to try to lock.
 * @return 0 on success, or an error code on failure.
 */
int		mtx_trylock(mtx_t *mtx);

/**
 * @brief Lock a mutex with a timeout.
 *
 * This function attempts to lock the specified mutex, blocking until the mutex is available or the timeout expires.
 *
 * @param mtx The mutex to lock.
 * @param ts The absolute time at which to time out.
 * @return 0 on success, thrd_timedout if the timeout expires, or an error code on failure.
 */
int		mtx_timedlock(mtx_t *mtx, const struct timespec *ts);

/**
 * @brief Unlock a mutex.
 *
 * This function unlocks the specified mutex.
 *
 * @param mtx The mutex to unlock.
 * @return 0 on success, or an error code on failure.
 */
int		mtx_unlock(mtx_t *mtx);

/**
 * @brief Destroy a mutex.
 *
 * This function destroys the specified mutex, releasing any resources it holds.
 *
 * @param mtx The mutex to destroy.
 */
void	mtx_destroy(mtx_t *mtx);

/* ----- Condition variables ----- */

/**
 * @brief Initialize a condition variable.
 *
 * This function initializes a condition variable.
 *
 * @param cond The condition variable to initialize.
 * @return 0 on success, or an error code on failure.
 */
int		cnd_init(cnd_t *cond);

/**
 * @brief Signal a condition variable.
 *
 * This function unblocks one thread waiting on the specified condition variable.
 *
 * @param cond The condition variable to signal.
 * @return 0 on success, or an error code on failure.
 */
int		cnd_signal(cnd_t *cond);

/**
 * @brief Broadcast a condition variable.
 *
 * This function unblocks all threads waiting on the specified condition variable.
 *
 * @param cond The condition variable to broadcast.
 * @return 0 on success, or an error code on failure.
 */
int		cnd_broadcast(cnd_t *cond);

/**
 * @brief Wait on a condition variable.
 *
 * This function atomically unlocks the specified mutex and waits for the condition variable to be signaled.
 * When the condition variable is signaled, the mutex is re-acquired before returning.
 *
 * @param cond The condition variable to wait on.
 * @param mtx The mutex that is locked by the calling thread.
 * @return 0 on success, or an error code on failure.
 */
int		cnd_wait(cnd_t *cond, mtx_t *mtx);

/**
 * @brief Wait on a condition variable with a timeout.
 *
 * This function atomically unlocks the specified mutex and waits for the condition variable to be signaled or for the absolute timeout specified by ts to be reached.
 * When the condition variable is signaled or the timeout expires, the mutex is re-acquired before returning.
 *
 * @param cond The condition variable to wait on.
 * @param mtx The mutex that is locked by the calling thread.
 * @param ts The absolute time at which to time out.
 * @return 0 on success, thrd_timedout if the timeout expires, or an error code on failure.
 */
int		cnd_timedwait(cnd_t *cond, mtx_t *mtx, const struct timespec *ts);

/**
 * @brief Destroy a condition variable.
 *
 * This function destroys the specified condition variable, releasing any resources it holds.
 *
 * @param cond The condition variable to destroy.
 */
void	cnd_destroy(cnd_t *cond);

/* ----- Thread-specific storage ----- */

/**
 * @brief Thread-specific storage destructor type.
 *
 * This type represents the signature of a destructor function for thread-specific storage.
 * A destructor function takes a single void pointer argument and returns nothing.
 */
typedef void (*tss_dtor_t)(void *);

/**
 * @brief Create a thread-specific storage key.
 *
 * This function creates a new thread-specific storage key that can be used to store and retrieve data specific to a thread.
 *
 * @param key Pointer to the key to be initialized.
 * @param dtor The destructor function to be called when a thread exits, or NULL if no destructor is needed.
 * @return 0 on success, or an error code on failure.
 */
int		tss_create(tss_t *key, tss_dtor_t dtor);

/**
 * @brief Delete a thread-specific storage key.
 *
 * This function deletes the specified thread-specific storage key, releasing any resources it holds.
 *
 * @param key The key to delete.
 */
void	tss_delete(tss_t key);

/**
 * @brief Get the value associated with a thread-specific storage key.
 *
 * This function retrieves the value associated with the specified thread-specific storage key for the calling thread.
 *
 * @param key The key to retrieve the value for.
 * @return The value associated with the key, or NULL if no value is set.
 */
void	*tss_get(tss_t key);

/**
 * @brief Set the value associated with a thread-specific storage key.
 *
 * This function sets the value associated with the specified thread-specific storage key for the calling thread.
 *
 * @param key The key to set the value for.
 * @param val The value to associate with the key.
 * @return 0 on success, or an error code on failure.
 */
int		tss_set(tss_t key, void *val);

/* ----- call_once ----- */

/**
 * @brief Call a function exactly once.
 *
 * This function ensures that the specified function is called exactly once, even if multiple threads attempt to call it concurrently.
 * The once_flag must be initialized to ONCE_FLAG_INIT before the first call to call_once.
 *
 * @param flag Pointer to the once_flag that controls the one-time execution.
 * @param func Pointer to the function to be called once.
 */
void	call_once(once_flag *flag, void (*func)(void));

# ifdef __cplusplus
}
# endif

#endif
