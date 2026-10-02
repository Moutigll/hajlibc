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
 * @Updated: 2026/10/02 11:47:33 by Moutig
 *
 * The pthread types (pthread_t, pthread_attr_t, ...) are
 * defined in <bits/thread/pthreadtypes.h>, pulled in
 * transitively through <bits/types.h>. Their internal layout
 * is private.
 */

#ifndef _PTHREAD_H
# define _PTHREAD_H

# include <stddef.h>
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
