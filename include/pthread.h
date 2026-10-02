/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file pthread.h
 * @brief POSIX threads API.
 * @Created: 2026/10/01 11:00:00 by Moutig
 * @Updated: 2026/10/02 14:53:10 by Moutig
 *
 * The pthread types (pthread_t, pthread_attr_t, ...) are
 * defined in <bits/thread/pthreadtypes.h>, pulled in
 * transitively through <bits/types.h>. Their internal layout
 * is private.
 */

#ifndef _PTHREAD_H
# define _PTHREAD_H

# include <stddef.h>
# include <time.h>
# include <bits/types.h>			/* pulls in pthreadtypes.h */
# include <bits/compiler.h>

# ifdef __cplusplus
extern "C" {
# endif

/* ----- Detach states ----- */
# define PTHREAD_CREATE_JOINABLE	0
# define PTHREAD_CREATE_DETACHED	1

/* ----- Scheduling ----- */
# define PTHREAD_INHERIT_SCHED		0
# define PTHREAD_EXPLICIT_SCHED		1
# define PTHREAD_SCOPE_SYSTEM		0
# define PTHREAD_SCOPE_PROCESS		1

/* ----- Cancellation ----- */
# define PTHREAD_CANCEL_ENABLE			0
# define PTHREAD_CANCEL_DISABLE			1
# define PTHREAD_CANCEL_DEFERRED		0
# define PTHREAD_CANCEL_ASYNCHRONOUS	1

/* ----- Mutex types ----- */
# ifndef PTHREAD_MUTEX_NORMAL
#  define PTHREAD_MUTEX_NORMAL		0
# endif
# ifndef PTHREAD_MUTEX_RECURSIVE
#  define PTHREAD_MUTEX_RECURSIVE	1
# endif
# ifndef PTHREAD_MUTEX_ERRORCHECK
#  define PTHREAD_MUTEX_ERRORCHECK	2
# endif
# ifndef PTHREAD_MUTEX_DEFAULT
#  define PTHREAD_MUTEX_DEFAULT		PTHREAD_MUTEX_NORMAL
# endif

# ifndef PTHREAD_MUTEX_STALLED
#  define PTHREAD_MUTEX_STALLED		0
# endif
# ifndef PTHREAD_MUTEX_ROBUST
#  define PTHREAD_MUTEX_ROBUST		1
# endif

/* ----- Process sharing ----- */
# ifndef PTHREAD_PROCESS_PRIVATE
#  define PTHREAD_PROCESS_PRIVATE	0
# endif
# ifndef PTHREAD_PROCESS_SHARED
#  define PTHREAD_PROCESS_SHARED	1
# endif

/* ----- Priority protocols ----- */
# ifndef PTHREAD_PRIO_NONE
#  define PTHREAD_PRIO_NONE		0
# endif
# ifndef PTHREAD_PRIO_INHERIT
#  define PTHREAD_PRIO_INHERIT		1
# endif
# ifndef PTHREAD_PRIO_PROTECT
#  define PTHREAD_PRIO_PROTECT		2
# endif

/**
 * @brief Initialiser for a pthread_once_t.
 *
 * A pthread_once_t must be initialised with this macro before
 * being passed to pthread_once.
 */
# define PTHREAD_ONCE_INIT	0

/**
 * @brief Scheduling parameters.
 *
 * Structure containing the scheduling parameters for a thread.
 *
 * @param sched_priority The priority of the thread.
 */
struct sched_param {
	int	sched_priority;
};

/* ----- Threads ----- */

/**
 * @brief Create a new thread.
 *
 * Creates a new thread of execution. The new thread starts by
 * invoking start_routine(arg). The thread's ID is stored in
 * *thread. If attr is NULL, default attributes are used.
 *
 * @param thread Pointer to a pthread_t to receive the new thread ID.
 * @param attr Pointer to a pthread_attr_t specifying thread attributes, or NULL for defaults.
 * @param start_routine Function pointer to the thread's start routine.
 * @param arg Argument to pass to the start routine.
 * @return 0 on success, or an error number on failure.
 */
int			pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);

/**
 * @brief Terminate the calling thread.
 *
 * Terminates the calling thread and makes its return value
 * available to any joining thread. This function does not
 * return.
 *
 * @param retval Pointer to the return value of the thread.
 */
void		pthread_exit(void *retval) __HAJ_NORETURN;

/**
 * @brief Wait for a thread to terminate.
 *
 * Waits for the thread specified by thread to terminate. If
 * retval is not NULL, the return value of the terminated thread
 * is stored in *retval. The thread must be joinable; joining a
 * detached thread results in undefined behavior.
 *
 * @param thread The ID of the thread to join.
 * @param retval Pointer to store the return value of the joined thread, or NULL.
 * @return 0 on success, or an error number on failure.
 */
int			pthread_join(pthread_t thread, void **retval);

/**
 * @brief Detach a thread.
 *
 * Marks the thread specified by thread as detached. Detached
 * threads automatically free their resources upon termination,
 * and cannot be joined. If the thread is already detached or
 * has terminated, this function has no effect.
 *
 * @param thread The ID of the thread to detach.
 * @return 0 on success, or an error number on failure.
 */
int			pthread_detach(pthread_t thread);

/**
 * @brief Get the calling thread's ID.
 *
 * Returns the ID of the calling thread. This is a unique
 * identifier for the thread within the process.
 *
 * @return The calling thread's ID.
 */
pthread_t	pthread_self(void);

/**
 * @brief Compare two thread IDs for equality.
 *
 * Compares two thread IDs. Returns a non-zero value if they
 * are equal, and 0 otherwise.
 *
 * @param t1 First thread ID to compare.
 * @param t2 Second thread ID to compare.
 * @return Non-zero if t1 and t2 are equal, 0 otherwise.
 */
int			pthread_equal(pthread_t t1, pthread_t t2);

/**
 * @brief Execute a function once.
 *
 * Ensures that the function pointed to by init_routine is executed
 * exactly once, regardless of how many threads call pthread_once
 * with the same once_control.
 *
 * @param once_control Pointer to a pthread_once_t control variable.
 * @param init_routine Function to execute once.
 * @return 0 on success, or an error number on failure.
 */
int pthread_once(pthread_once_t *once_control, void (*init_routine)(void));





/* ----- Thread-specific data (TSD) ----- */

/**
 * @brief Create a thread-specific data key.
 *
 * Creates a new thread-specific data key visible to all threads.
 * The key can be used to store and retrieve per-thread values.
 * If destructor is not NULL, it is called with the value associated
 * with the key when a thread exits, if that value is non-NULL.
 *
 * @param key Pointer to a pthread_key_t to receive the new key.
 * @param destructor Optional destructor function for the key's values.
 * @return 0 on success, or an error number on failure.
 */
int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));

/**
 * @brief Delete a thread-specific data key.
 *
 * Deletes the thread-specific data key specified by key. After
 * deletion, the key is no longer valid and cannot be used to
 * access thread-specific data. Any values associated with the
 * key in existing threads are not affected.
 *
 * @param key The pthread_key_t to delete.
 * @return 0 on success, or an error number on failure.
 */
int pthread_key_delete(pthread_key_t key);

/**
 * @brief Set the value of a thread-specific data key.
 *
 * Sets the value of the thread-specific data key specified by key
 * to the value pointed to by value. If a value is already associated
 * with the key in the calling thread, it is replaced.
 *
 * @param key The pthread_key_t to set.
 * @param value The value to set.
 * @return 0 on success, or an error number on failure.
 */
int pthread_setspecific(pthread_key_t key, const void *value);

/**
 * @brief Get the value of a thread-specific data key.
 *
 * Retrieves the value of the thread-specific data key specified by key
 * in the calling thread. If no value is associated with the key in the
 * calling thread, NULL is returned.
 *
 * @param key The pthread_key_t to get.
 * @return The value associated with the key, or NULL if no value is associated.
 */
void *pthread_getspecific(pthread_key_t key);





/* ----- Mutexes ----- */

/**
 * @brief Initialize a mutex.
 *
 * Initializes the mutex pointed to by mutex with the attributes
 * specified by attr. If attr is NULL, default attributes are used.
 *
 * @param mutex Pointer to a pthread_mutex_t to initialize.
 * @param attr Pointer to a pthread_mutexattr_t specifying mutex attributes, or NULL for defaults.
 * @return 0 on success, or an error number on failure.
 */
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);

/**
 * @brief Destroy a mutex.
 *
 * Destroys the mutex pointed to by mutex, freeing any resources
 * it may hold. The mutex should not be used after this call.
 *
 * @param mutex Pointer to a pthread_mutex_t to destroy.
 * @return 0 on success, or an error number on failure.
 */
int pthread_mutex_destroy(pthread_mutex_t *mutex);

/**
 * @brief Lock a mutex.
 *
 * Locks the mutex pointed to by mutex. If the mutex is already
 * locked, the calling thread blocks until the mutex becomes
 * available.
 * If the mutex is recursive and the calling thread already owns it, the lock count is incremented.
 *
 * @param mutex Pointer to a pthread_mutex_t to lock.
 * @return 0 on success, or an error number on failure.
 */
int pthread_mutex_lock(pthread_mutex_t *mutex);

/**
 * @brief Try to lock a mutex.
 *
 * Attempts to lock the mutex pointed to by mutex. If the mutex is already
 * locked, the function returns immediately with an error.
 * If the mutex is recursive and the calling thread already owns it, the lock count is incremented.
 *
 * @param mutex Pointer to a pthread_mutex_t to try to lock.
 * @return 0 on success, or an error number on failure.
 */
int pthread_mutex_trylock(pthread_mutex_t *mutex);

/**
 * @brief Lock a mutex with a timeout.
 *
 * Locks the mutex pointed to by mutex. If the mutex is already
 * locked, the calling thread blocks until the mutex becomes
 * available or the absolute timeout specified by abstime is
 * reached.
 *
 * @param mutex   Pointer to a pthread_mutex_t to lock.
 * @param abstime Absolute timeout, or NULL for no timeout.
 * @return 0 on success, ETIMEDOUT on timeout, or an error
 *         number on failure.
 */
int pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *abstime);

/**
 * @brief Unlock a mutex.
 *
 * Unlocks the mutex pointed to by mutex. If the mutex is recursive and
 * the calling thread owns it, the lock count is decremented. If the lock
 * count reaches zero, the mutex is released.
 *
 * @param mutex Pointer to a pthread_mutex_t to unlock.
 * @return 0 on success, or an error number on failure.
 */
int pthread_mutex_unlock(pthread_mutex_t *mutex);





/* ----- Condition variables ----- */

/**
 * @brief Initialize a condition variable.
 *
 * Initializes the condition variable pointed to by cond with the attributes
 * specified by attr. If attr is NULL, default attributes are used.
 *
 * @param cond Pointer to a pthread_cond_t to initialize.
 * @param attr Pointer to a pthread_condattr_t specifying condition variable attributes, or NULL for defaults.
 * @return 0 on success, or an error number on failure.
 */
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);

/**
 * @brief Destroy a condition variable.
 *
 * Destroys the condition variable pointed to by cond, freeing any resources
 * it may hold. The condition variable should not be used after this call.
 *
 * @param cond Pointer to a pthread_cond_t to destroy.
 * @return 0 on success, or an error number on failure.
 */
int pthread_cond_destroy(pthread_cond_t *cond);

/**
 * @brief Wait on a condition variable.
 *
 * Atomically unlocks the mutex and waits for the condition variable to be
 * signaled. When the condition variable is signaled, the mutex is re-acquired
 * before returning. The mutex must be locked by the calling thread before
 * calling this function.
 *
 * @param cond  Pointer to a pthread_cond_t to wait on.
 * @param mutex Pointer to a pthread_mutex_t that is locked by the calling thread.
 * @return 0 on success, or an error number on failure.
 */
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);

/**
 * @brief Wait on a condition variable with a timeout.
 *
 * Atomically unlocks the mutex and waits for the condition variable to be
 * signaled or for the absolute timeout specified by abstime to be reached.
 * When the condition variable is signaled or the timeout expires, the mutex
 * is re-acquired before returning. The mutex must be locked by the calling
 * thread before calling this function.
 *
 * @param cond     Pointer to a pthread_cond_t to wait on.
 * @param mutex    Pointer to a pthread_mutex_t that is locked by the calling thread.
 * @param abstime  Absolute timeout, or NULL for no timeout.
 * @return 0 on success, ETIMEDOUT on timeout, or an error number on failure.
 */
int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime);

/**
 * @brief Wait on a condition variable with a specified clock.
 *
 * Atomically unlocks the mutex and waits for the condition variable to be
 * signaled or for the absolute timeout specified by abstime to be reached,
 * using the specified clock. When the condition variable is signaled or the
 * timeout expires, the mutex is re-acquired before returning. The mutex must
 * be locked by the calling thread before calling this function.
 *
 * @param cond     Pointer to a pthread_cond_t to wait on.
 * @param mutex    Pointer to a pthread_mutex_t that is locked by the calling thread.
 * @param clockid  Clock ID (CLOCK_REALTIME or CLOCK_MONOTONIC) to use for the timeout.
 * @param abstime  Absolute timeout, or NULL for no timeout.
 * @return 0 on success, ETIMEDOUT on timeout, or an error number on failure.
 */
int pthread_cond_clockwait(pthread_cond_t *cond, pthread_mutex_t *mutex, clockid_t clockid, const struct timespec *abstime);

/**
 * @brief Signal a condition variable.
 *
 * Signals the condition variable pointed to by cond, waking up one waiting thread.
 * If no threads are waiting, the signal is lost.
 *
 * @param cond Pointer to a pthread_cond_t to signal.
 * @return 0 on success, or an error number on failure.
 */
int pthread_cond_signal(pthread_cond_t *cond);

/**
 * @brief Broadcast a condition variable.
 *
 * Broadcasts the condition variable pointed to by cond, waking up all waiting threads.
 * If no threads are waiting, the broadcast is lost.
 *
 * @param cond Pointer to a pthread_cond_t to broadcast.
 * @return 0 on success, or an error number on failure.
 */
int pthread_cond_broadcast(pthread_cond_t *cond);










/* ----- Attributes ----- */

/**
 * @brief Initialize a thread attributes object.
 *
 * Initializes the thread attributes object pointed to by attr
 * with default values. The object can then be modified using
 * other pthread_attr_* functions.
 *
 * @param attr Pointer to a pthread_attr_t to initialize.
 * @return 0 on success, or an error number on failure.
 */
int pthread_attr_init(pthread_attr_t *attr);

/**
 * @brief Destroy a thread attributes object.
 *
 * Destroys the thread attributes object pointed to by attr,
 * freeing any resources it may hold. The object should not be
 * used after this call.
 *
 * @param attr Pointer to a pthread_attr_t to destroy.
 * @return 0 on success, or an error number on failure.
 */
int pthread_attr_destroy(pthread_attr_t *attr);

/* ----- Detach state ----- */
/**
 * @brief Set the detach state of a thread attributes object.
 *
 * Sets the detach state of the thread attributes object pointed to by attr.
 * The detach state determines whether the thread will be joinable or detached.
 *
 * @param attr Pointer to a pthread_attr_t to modify.
 * @param state The new detach state (PTHREAD_CREATE_JOINABLE or PTHREAD_CREATE_DETACHED).
 * @return 0 on success, or an error number on failure.
 */
int pthread_attr_setdetachstate(pthread_attr_t *attr, int state);
/**
 * @brief Get the detach state of a thread attributes object.
 *
 * Retrieves the detach state of the thread attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_attr_t to query.
 * @param state Pointer to an int to store the detach state.
 * @return 0 on success, or an error number on failure.
 */
int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *state);

/* ----- Stack size ----- */
/**
 * @brief Set the stack size of a thread attributes object.
 *
 * Sets the stack size of the thread attributes object pointed to by attr.
 * The stack size must be at least PTHREAD_STACK_MIN.
 *
 * @param attr Pointer to a pthread_attr_t to modify.
 * @param size The new stack size in bytes.
 * @return 0 on success, or an error number on failure.
 */
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t size);
/**
 * @brief Get the stack size of a thread attributes object.
 *
 * Retrieves the stack size of the thread attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_attr_t to query.
 * @param size Pointer to a size_t to store the stack size.
 * @return 0 on success, or an error number on failure.
 */
int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *size);

/* ----- Guard size ----- */
/**
 * @brief Set the guard size of a thread attributes object.
 *
 * Sets the guard size of the thread attributes object pointed to by attr.
 * The guard size is the size of the guard area at the end of the thread's stack,
 * which is used to detect stack overflows.
 * The guard size must be a multiple of the system's page size.
 *
 * @param attr Pointer to a pthread_attr_t to modify.
 * @param size The new guard size in bytes.
 * @return 0 on success, or an error number on failure.
 */
int pthread_attr_setguardsize(pthread_attr_t *attr, size_t size);
/**
 * @brief Get the guard size of a thread attributes object.
 *
 * Retrieves the guard size of the thread attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_attr_t to query.
 * @param size Pointer to a size_t to store the guard size.
 * @return 0 on success, or an error number on failure.
 */
int pthread_attr_getguardsize(const pthread_attr_t *attr, size_t *size);

/* ----- Stack address ----- */
/**
 * @brief Set the stack address and size of a thread attributes object.
 *
 * Sets the stack address and size of the thread attributes object pointed to by attr.
 * The stack address must be aligned to a multiple of the system's page size.
 * The stack size must be at least PTHREAD_STACK_MIN.
 *
 * @param attr Pointer to a pthread_attr_t to modify.
 * @param stackaddr Pointer to the new stack address.
 * @param stacksize The new stack size in bytes.
 * @return 0 on success, or an error number on failure.
 */
int pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr, size_t stacksize);
/**
 * @brief Get the stack address and size of a thread attributes object.
 *
 * Retrieves the stack address and size of the thread attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_attr_t to query.
 * @param stackaddr Pointer to a void* to store the stack address.
 * @param stacksize Pointer to a size_t to store the stack size.
 * @return 0 on success, or an error number on failure.
 */
int pthread_attr_getstack(const pthread_attr_t *attr, void **stackaddr, size_t *stacksize);

/* ----- Scheduling policy ----- */
/**
 * @brief Set the scheduling policy of a thread attributes object.
 *
 * Sets the scheduling policy of the thread attributes object pointed to by attr.
 * The policy must be one of HAJ_SCHED_OTHER, HAJ_SCHED_FIFO, or HAJ_SCHED_RR.
 *
 * @param attr Pointer to a pthread_attr_t to modify.
 * @param policy The new scheduling policy.
 * @return 0 on success, or an error number on failure.
 */
int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy);
/**
 * @brief Get the scheduling policy of a thread attributes object.
 *
 * Retrieves the scheduling policy of the thread attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_attr_t to query.
 * @param policy Pointer to an int to store the scheduling policy.
 * @return 0 on success, or an error number on failure.
 */
int pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy);

/* ----- Scheduling priority ----- */
/**
 * @brief Set the scheduling parameters of a thread attributes object.
 *
 * Sets the scheduling parameters (priority) of the thread attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_attr_t to modify.
 * @param param Pointer to a struct sched_param containing the new scheduling parameters.
 * @return 0 on success, or an error number on failure.
 */
int pthread_attr_setschedparam(pthread_attr_t *attr, const struct sched_param *param);
/**
 * @brief Get the scheduling parameters of a thread attributes object.
 *
 * Retrieves the scheduling parameters (priority) of the thread attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_attr_t to query.
 * @param param Pointer to a struct sched_param to store the scheduling parameters.
 * @return 0 on success, or an error number on failure.
 */
int pthread_attr_getschedparam(const pthread_attr_t *attr, struct sched_param *param);

/* ----- Inherit scheduler ----- */
/**
 * @brief Set the inherit scheduler attribute of a thread attributes object.
 *
 * Sets whether the thread will inherit its scheduling attributes from the
 * creating thread or use the explicitly set attributes.
 *
 * @param attr Pointer to a pthread_attr_t to modify.
 * @param inherit The new inherit scheduler attribute (PTHREAD_INHERIT_SCHED or PTHREAD_EXPLICIT_SCHED).
 * @return 0 on success, or an error number on failure.
 */
int pthread_attr_setinheritsched(pthread_attr_t *attr, int inherit);
/**
 * @brief Get the inherit scheduler attribute of a thread attributes object.
 *
 * Retrieves whether the thread will inherit its scheduling attributes from the
 * creating thread or use the explicitly set attributes.
 *
 * @param attr Pointer to a pthread_attr_t to query.
 * @param inherit Pointer to an int to store the inherit scheduler attribute.
 * @return 0 on success, or an error number on failure.
 */
int pthread_attr_getinheritsched(const pthread_attr_t *attr, int *inherit);

/* ----- Scope ----- */
/**
 * @brief Set the contention scope of a thread attributes object.
 *
 * Sets the contention scope of the thread attributes object pointed to by attr.
 * The scope determines whether the thread competes for CPU time with all threads
 * in the system (PTHREAD_SCOPE_SYSTEM) or only with threads in the same process (PTHREAD_SCOPE_PROCESS).
 *
 * @param attr Pointer to a pthread_attr_t to modify.
 * @param scope The new contention scope (PTHREAD_SCOPE_SYSTEM or PTHREAD_SCOPE_PROCESS).
 * @return 0 on success, or an error number on failure.
 */
int pthread_attr_setscope(pthread_attr_t *attr, int scope);
/**
 * @brief Get the contention scope of a thread attributes object.
 *
 * Retrieves the contention scope of the thread attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_attr_t to query.
 * @param scope Pointer to an int to store the contention scope.
 * @return 0 on success, or an error number on failure.
 */
int pthread_attr_getscope(const pthread_attr_t *attr, int *scope);





/* ----- Mutex attributes ----- */

/**
 * @brief Initialize a mutex attributes object.
 *
 * Initializes the mutex attributes object pointed to by attr
 * with default values. The object can then be modified using
 * other pthread_mutexattr_* functions.
 *
 * @param attr Pointer to a pthread_mutexattr_t to initialize.
 * @return 0 on success, or an error number on failure.
 */
int pthread_mutexattr_init(pthread_mutexattr_t *attr);

/**
 * @brief Destroy a mutex attributes object.
 *
 * Destroys the mutex attributes object pointed to by attr,
 * freeing any resources it may hold. The object should not be
 * used after this call.
 *
 * @param attr Pointer to a pthread_mutexattr_t to destroy.
 * @return 0 on success, or an error number on failure.
 */
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);

/**
 * @brief Set the type of a mutex attributes object.
 *
 * Sets the type of the mutex attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_mutexattr_t to modify.
 * @param type The new mutex type.
 * @return 0 on success, or an error number on failure.
 */
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type);

/**
 * @brief Get the type of a mutex attributes object.
 *
 * Retrieves the type of the mutex attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_mutexattr_t to query.
 * @param type Pointer to an int to store the mutex type.
 * @return 0 on success, or an error number on failure.
 */
int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type);

/**
 * @brief Set the process-shared attribute of a mutex attributes object.
 *
 * Sets the process-shared attribute of the mutex attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_mutexattr_t to modify.
 * @param pshared The new process-shared attribute.
 * @return 0 on success, or an error number on failure.
 */
int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared);

/**
 * @brief Get the process-shared attribute of a mutex attributes object.
 *
 * Retrieves the process-shared attribute of the mutex attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_mutexattr_t to query.
 * @param pshared Pointer to an int to store the process-shared attribute.
 * @return 0 on success, or an error number on failure.
 */
int pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr, int *pshared);

/**
 * @brief Set the robustness attribute of a mutex attributes object.
 *
 * Sets the robustness attribute of the mutex attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_mutexattr_t to modify.
 * @param robust The new robustness attribute.
 * @return 0 on success, or an error number on failure.
 */
int pthread_mutexattr_setrobust(pthread_mutexattr_t *attr, int robust);

/**
 * @brief Get the robustness attribute of a mutex attributes object.
 *
 * Retrieves the robustness attribute of the mutex attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_mutexattr_t to query.
 * @param robust Pointer to an int to store the robustness attribute.
 * @return 0 on success, or an error number on failure.
 */
int pthread_mutexattr_getrobust(const pthread_mutexattr_t *attr, int *robust);

/**
 * @brief Set the protocol attribute of a mutex attributes object.
 *
 * Sets the protocol attribute of the mutex attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_mutexattr_t to modify.
 * @param protocol The new protocol attribute.
 * @return 0 on success, or an error number on failure.
 */
int pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr, int protocol);

/**
 * @brief Get the protocol attribute of a mutex attributes object.
 *
 * Retrieves the protocol attribute of the mutex attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_mutexattr_t to query.
 * @param protocol Pointer to an int to store the protocol attribute.
 * @return 0 on success, or an error number on failure.
 */
int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *attr, int *protocol);

/**
 * @brief Set the prioceiling attribute of a mutex attributes object.
 *
 * Sets the prioceiling attribute of the mutex attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_mutexattr_t to modify.
 * @param prioceiling The new prioceiling attribute.
 * @return 0 on success, or an error number on failure.
 */
int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *attr, int prioceiling);

/**
 * @brief Get the prioceiling attribute of a mutex attributes object.
 *
 * Retrieves the prioceiling attribute of the mutex attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_mutexattr_t to query.
 * @param prioceiling Pointer to an int to store the prioceiling attribute.
 * @return 0 on success, or an error number on failure.
 */
int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *attr, int *prioceiling);





/* ---- Condition variable attributes ----- */
/**
 * @brief Initialize a condition variable attributes object.
 *
 * Initializes the condition variable attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_condattr_t to initialize.
 * @return 0 on success, or an error number on failure.
 */
int pthread_condattr_init(pthread_condattr_t *attr);

/**
 * @brief Destroy a condition variable attributes object.
 *
 * Destroys the condition variable attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_condattr_t to destroy.
 * @return 0 on success, or an error number on failure.
 */
int pthread_condattr_destroy(pthread_condattr_t *attr);

/**
 * @brief Set the process-shared attribute of a condition variable attributes object.
 *
 * Sets the process-shared attribute of the condition variable attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_condattr_t to modify.
 * @param pshared The new process-shared attribute.
 * @return 0 on success, or an error number on failure.
 */
int pthread_condattr_setpshared(pthread_condattr_t *attr, int pshared);

/**
 * @brief Get the process-shared attribute of a condition variable attributes object.
 *
 * Retrieves the process-shared attribute of the condition variable attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_condattr_t to query.
 * @param pshared Pointer to an int to store the process-shared attribute.
 * @return 0 on success, or an error number on failure.
 */
int pthread_condattr_getpshared(const pthread_condattr_t *attr, int *pshared);

/**
 * @brief Set the clock attribute of a condition variable attributes object.
 *
 * Sets the clock attribute of the condition variable attributes object pointed to by attr.
 * The clock attribute determines which clock is used for timed waits on the condition variable.
 *
 * @param attr Pointer to a pthread_condattr_t to modify.
 * @param clockid The new clock ID (e.g., CLOCK_REALTIME or CLOCK_MONOTONIC).
 * @return 0 on success, or an error number on failure.
 */
int pthread_condattr_setclock(pthread_condattr_t *attr, clockid_t clockid);

/**
 * @brief Get the clock attribute of a condition variable attributes object.
 *
 * Retrieves the clock attribute of the condition variable attributes object pointed to by attr.
 *
 * @param attr Pointer to a pthread_condattr_t to query.
 * @param clockid Pointer to a clockid_t to store the clock ID.
 * @return 0 on success, or an error number on failure.
 */
int pthread_condattr_getclock(const pthread_condattr_t *attr, clockid_t *clockid);





/* ----- Initializers ----- */

/**
 * @brief Initializer for a pthread_mutex_t.
 *
 * This macro can be used to statically initialize a mutex
 * object. It expands to an initializer for a pthread_mutex_t
 * with default attributes.
 */
# define PTHREAD_MUTEX_INITIALIZER	{ { 0 } }

/**
 * @brief Initializer for a pthread_cond_t.
 *
 * This macro can be used to statically initialize a condition
 * variable object. It expands to an initializer for a
 * pthread_cond_t with default attributes.
 */
# define PTHREAD_COND_INITIALIZER	{ { 0 } }

# ifdef __cplusplus
}
# endif

#endif /* _PTHREAD_H */
