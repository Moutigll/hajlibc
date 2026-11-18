/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file stdatomic.h
 * @brief C11 atomic operations and types.
 * @Created: 2026/10/05 15:35:29 by Moutig
 * @Updated: 2026/10/05 16:38:02 by Moutig
 *
 * Full C11 <stdatomic.h> implementation on top of the GCC and
 * Clang atomic builtins. No external function is required:
 * every operation lowers to a compiler builtin, which becomes
 * either a lock-free instruction or a small call to libgcc
 * (for the rare 128-bit cases).
 *
 * The two compilers do not expose the same builtins for the
 * pointer argument:
 *
 *   - GCC's __atomic_* accept both `T *` and `_Atomic(T) *`
 *     and are therefore usable directly.
 *
 *   - Clang's __atomic_* are stricter and reject
 *     `_Atomic(T) *`, so we use Clang's __c11_atomic_*
 *     builtins, which are exactly what Clang's own
 *     <stdatomic.h> uses.
 *
 * The types are declared with the _Atomic qualifier, which
 * the compiler treats as a distinct type in C11 and as a
 * transparent alias in older dialects.
 *
 * Reference: ISO/IEC 9899:2011, section 7.17.
 */

#ifndef _STDATOMIC_H
# define _STDATOMIC_H

# include <bits/types.h>

# if defined(__cplusplus)
extern "C" {
# endif

# if !defined(__GNUC__) && !defined(__clang__)
#  error "stdatomic.h requires GCC or Clang for atomic builtins"
# endif

/* ----- memory_order ----- */

/**
 * @brief Memory ordering for atomic operations.
 *
 * The numeric values match the GCC __ATOMIC_* constants, so
 * the builtins can be called directly with these values.
 */
typedef enum {
	memory_order_relaxed = __ATOMIC_RELAXED,
	memory_order_consume = __ATOMIC_CONSUME,
	memory_order_acquire = __ATOMIC_ACQUIRE,
	memory_order_release = __ATOMIC_RELEASE,
	memory_order_acq_rel = __ATOMIC_ACQ_REL,
	memory_order_seq_cst = __ATOMIC_SEQ_CST
} memory_order;

/* ----- Atomic integer types ----- */

typedef _Atomic _Bool					atomic_bool;
typedef _Atomic char					atomic_char;
typedef _Atomic __CHAR16_TYPE__			atomic_char16_t;
typedef _Atomic __CHAR32_TYPE__			atomic_char32_t;
typedef _Atomic __WCHAR_TYPE__			atomic_wchar_t;
typedef _Atomic signed char				atomic_schar;
typedef _Atomic unsigned char			atomic_uchar;
typedef _Atomic short					atomic_short;
typedef _Atomic unsigned short			atomic_ushort;
typedef _Atomic int						atomic_int;
typedef _Atomic unsigned int			atomic_uint;
typedef _Atomic long					atomic_long;
typedef _Atomic unsigned long			atomic_ulong;
typedef _Atomic long long				atomic_llong;
typedef _Atomic unsigned long long		atomic_ullong;

typedef _Atomic __INT_LEAST8_TYPE__		atomic_int_least8_t;
typedef _Atomic __UINT_LEAST8_TYPE__	atomic_uint_least8_t;
typedef _Atomic __INT_LEAST16_TYPE__	atomic_int_least16_t;
typedef _Atomic __UINT_LEAST16_TYPE__	atomic_uint_least16_t;
typedef _Atomic __INT_LEAST32_TYPE__	atomic_int_least32_t;
typedef _Atomic __UINT_LEAST32_TYPE__	atomic_uint_least32_t;
typedef _Atomic __INT_LEAST64_TYPE__	atomic_int_least64_t;
typedef _Atomic __UINT_LEAST64_TYPE__	atomic_uint_least64_t;

typedef _Atomic __INT_FAST8_TYPE__		atomic_int_fast8_t;
typedef _Atomic __UINT_FAST8_TYPE__		atomic_uint_fast8_t;
typedef _Atomic __INT_FAST16_TYPE__		atomic_int_fast16_t;
typedef _Atomic __UINT_FAST16_TYPE__	atomic_uint_fast16_t;
typedef _Atomic __INT_FAST32_TYPE__		atomic_int_fast32_t;
typedef _Atomic __UINT_FAST32_TYPE__	atomic_uint_fast32_t;
typedef _Atomic __INT_FAST64_TYPE__		atomic_int_fast64_t;
typedef _Atomic __UINT_FAST64_TYPE__	atomic_uint_fast64_t;

typedef _Atomic __INTPTR_TYPE__			atomic_intptr_t;
typedef _Atomic __UINTPTR_TYPE__		atomic_uintptr_t;
typedef _Atomic __SIZE_TYPE__			atomic_size_t;
typedef _Atomic __PTRDIFF_TYPE__		atomic_ptrdiff_t;
typedef _Atomic __INTMAX_TYPE__			atomic_intmax_t;
typedef _Atomic __UINTMAX_TYPE__		atomic_uintmax_t;

# define ATOMIC_BOOL_LOCK_FREE		__GCC_ATOMIC_BOOL_LOCK_FREE
# define ATOMIC_CHAR_LOCK_FREE		__GCC_ATOMIC_CHAR_LOCK_FREE
# define ATOMIC_CHAR16_T_LOCK_FREE	__GCC_ATOMIC_CHAR16_T_LOCK_FREE
# define ATOMIC_CHAR32_T_LOCK_FREE	__GCC_ATOMIC_CHAR32_T_LOCK_FREE
# define ATOMIC_WCHAR_T_LOCK_FREE	__GCC_ATOMIC_WCHAR_T_LOCK_FREE
# define ATOMIC_SHORT_LOCK_FREE		__GCC_ATOMIC_SHORT_LOCK_FREE
# define ATOMIC_INT_LOCK_FREE		__GCC_ATOMIC_INT_LOCK_FREE
# define ATOMIC_LONG_LOCK_FREE		__GCC_ATOMIC_LONG_LOCK_FREE
# define ATOMIC_LLONG_LOCK_FREE		__GCC_ATOMIC_LLONG_LOCK_FREE
# define ATOMIC_POINTER_LOCK_FREE	__GCC_ATOMIC_POINTER_LOCK_FREE

/* ----- Atomic flag ----- */

/**
 * @brief Atomic boolean flag.
 *
 * atomic_flag is the only atomic type guaranteed to be
 * lock-free by the C11 standard. Use it for spinlocks and
 * one-shot initializations.
 */
typedef struct {
	_Bool	__val;
} atomic_flag;

/**
 * @brief Static initializer for atomic_flag.
 */
# define ATOMIC_FLAG_INIT	{ 0 }

/**
 * @brief Static initializer for any atomic object.
 *
 * C11 defines this macro, but it is not needed in practice:
 * static atomic objects are initialized by the compiler from
 * their constant initializer, and dynamic initialization uses
 * atomic_init().
 */
# define ATOMIC_VAR_INIT(value)	(value)

/* ----- Compiler dispatch ----- */

/*
 * The two compilers do not agree on whether the __atomic_*
 * builtins accept `_Atomic(T) *`. GCC does, Clang does not.
 * Clang provides __c11_atomic_* which are exactly the
 * builtins used by its own <stdatomic.h>, and which accept
 * `_Atomic(T) *` by design. We dispatch on the compiler and
 * define the same public macro names either way.
 */
#if defined(__clang__)

/* ----- Clang: __c11_atomic_* ----- */

/**
 * @brief Initialize an atomic object (non-atomically).
 *
 * Must not be called concurrently with any atomic access to
 * the same object.
 */
# define atomic_init(obj, value) \
	__c11_atomic_init((obj), (value))

/**
 * @brief Full memory fence for inter-thread ordering.
 */
# define atomic_thread_fence(order) \
	__c11_atomic_thread_fence((order))

/**
 * @brief Compiler-only fence for signal handlers.
 */
# define atomic_signal_fence(order) \
	__c11_atomic_signal_fence((order))

/**
 * @brief Return non-zero if the object's operations are lock-free.
 */
# define atomic_is_lock_free(obj) \
	__c11_atomic_is_lock_free(sizeof(*(obj)))

# define atomic_load_explicit(obj, order) \
	__c11_atomic_load((obj), (order))

# define atomic_store_explicit(obj, val, order) \
	__c11_atomic_store((obj), (val), (order))

# define atomic_exchange_explicit(obj, val, order) \
	__c11_atomic_exchange((obj), (val), (order))

# define atomic_compare_exchange_strong_explicit(obj, expected, desired, success, failure) \
	__c11_atomic_compare_exchange_strong((obj), (expected), (desired), \
										 (success), (failure))

# define atomic_compare_exchange_weak_explicit(obj, expected, desired, success, failure) \
	__c11_atomic_compare_exchange_weak((obj), (expected), (desired), \
									   (success), (failure))

# define atomic_fetch_add_explicit(obj, val, order) \
	__c11_atomic_fetch_add((obj), (val), (order))

# define atomic_fetch_sub_explicit(obj, val, order) \
	__c11_atomic_fetch_sub((obj), (val), (order))

# define atomic_fetch_or_explicit(obj, val, order) \
	__c11_atomic_fetch_or((obj), (val), (order))

# define atomic_fetch_xor_explicit(obj, val, order) \
	__c11_atomic_fetch_xor((obj), (val), (order))

# define atomic_fetch_and_explicit(obj, val, order) \
	__c11_atomic_fetch_and((obj), (val), (order))

/*
 * Clang has no __c11_atomic_test_and_set / clear. We
 * implement them with an exchange and a store on the inner
 * _Bool, which is exactly what the flag is.
 */
# define atomic_flag_test_and_set_explicit(flag, order) \
	__c11_atomic_exchange(&(flag)->__val, 1, (order))

# define atomic_flag_clear_explicit(flag, order) \
	__c11_atomic_store(&(flag)->__val, 0, (order))

#else  /* GCC */

/* ----- GCC: __atomic_* ----- */

/**
 * @brief Initialize an atomic object (non-atomically).
 *
 * Must not be called concurrently with any atomic access to
 * the same object.
 */
# define atomic_init(obj, value) \
	__atomic_store_n((obj), (value), __ATOMIC_RELAXED)

/**
 * @brief Full memory fence for inter-thread ordering.
 */
# define atomic_thread_fence(order) \
	__atomic_thread_fence(order)

/**
 * @brief Compiler-only fence for signal handlers.
 */
# define atomic_signal_fence(order) \
	__atomic_signal_fence(order)

/**
 * @brief Return non-zero if the object's operations are lock-free.
 */
# define atomic_is_lock_free(obj) \
	__atomic_is_lock_free(sizeof(*(obj)), (obj))

# define atomic_load_explicit(obj, order) \
	__atomic_load_n((obj), (order))

# define atomic_store_explicit(obj, val, order) \
	__atomic_store_n((obj), (val), (order))

# define atomic_exchange_explicit(obj, val, order) \
	__atomic_exchange_n((obj), (val), (order))

# define atomic_compare_exchange_strong_explicit(obj, expected, desired, success, failure) \
	__atomic_compare_exchange_n((obj), (expected), (desired), 0, \
								(success), (failure))

# define atomic_compare_exchange_weak_explicit(obj, expected, desired, success, failure) \
	__atomic_compare_exchange_n((obj), (expected), (desired), 1, \
								(success), (failure))

# define atomic_fetch_add_explicit(obj, val, order) \
	__atomic_fetch_add((obj), (val), (order))

# define atomic_fetch_sub_explicit(obj, val, order) \
	__atomic_fetch_sub((obj), (val), (order))

# define atomic_fetch_or_explicit(obj, val, order) \
	__atomic_fetch_or((obj), (val), (order))

# define atomic_fetch_xor_explicit(obj, val, order) \
	__atomic_fetch_xor((obj), (val), (order))

# define atomic_fetch_and_explicit(obj, val, order) \
	__atomic_fetch_and((obj), (val), (order))

# define atomic_flag_test_and_set_explicit(flag, order) \
	__atomic_test_and_set(&(flag)->__val, (order))

# define atomic_flag_clear_explicit(flag, order) \
	__atomic_clear(&(flag)->__val, (order))

#endif  /* __clang__ */

/* ----- kill_dependency ----- */

/**
 * @brief Tell the compiler that a value carries a data dependency.
 *
 * Purely a hint. Returns its argument unchanged.
 */
# define kill_dependency(y)	(y)

/* ----- Sequential consistency convenience macros ----- */

# define atomic_load(obj) \
	atomic_load_explicit((obj), memory_order_seq_cst)

# define atomic_store(obj, val) \
	atomic_store_explicit((obj), (val), memory_order_seq_cst)

# define atomic_exchange(obj, val) \
	atomic_exchange_explicit((obj), (val), memory_order_seq_cst)

# define atomic_compare_exchange_strong(obj, expected, desired) \
	atomic_compare_exchange_strong_explicit((obj), (expected), (desired), \
											memory_order_seq_cst, \
											memory_order_seq_cst)

# define atomic_compare_exchange_weak(obj, expected, desired) \
	atomic_compare_exchange_weak_explicit((obj), (expected), (desired), \
										  memory_order_seq_cst, \
										  memory_order_seq_cst)

# define atomic_fetch_add(obj, val) \
	atomic_fetch_add_explicit((obj), (val), memory_order_seq_cst)

# define atomic_fetch_sub(obj, val) \
	atomic_fetch_sub_explicit((obj), (val), memory_order_seq_cst)

# define atomic_fetch_or(obj, val) \
	atomic_fetch_or_explicit((obj), (val), memory_order_seq_cst)

# define atomic_fetch_xor(obj, val) \
	atomic_fetch_xor_explicit((obj), (val), memory_order_seq_cst)

# define atomic_fetch_and(obj, val) \
	atomic_fetch_and_explicit((obj), (val), memory_order_seq_cst)

# define atomic_flag_test_and_set(flag) \
	atomic_flag_test_and_set_explicit((flag), memory_order_seq_cst)

# define atomic_flag_clear(flag) \
	atomic_flag_clear_explicit((flag), memory_order_seq_cst)

# if defined(__cplusplus)
}
# endif

#endif /* _STDATOMIC_H */
