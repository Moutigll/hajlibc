/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file tls.h
 * @brief Thread-Local Storage (TLS) internals.
 * @Created: 2026/09/30 05:17:45 by Moutig
 * @Updated: 2026/10/01 11:55:35 by Moutig
 *
 * hajlib implements the ELF TLS ABI with the TLS_TCB_AT_TP model:
 * the TCB is placed at the *top* of the thread's TLS block, and
 * the thread register (%fs on x86_64, tpidr_el0 on aarch64)
 * points to it. The TLS variables live at negative offsets from
 * the thread register.
 *
 * Layout of the TLS block for one thread:
 *
 *     high address
 *     +-------------------------+
 *     |      TCB                |  <- thread register points here
 *     +-------------------------+
 *     |      .tbss              |  <- zero-initialized TLS vars
 *     +-------------------------+
 *     |      .tdata             |  <- pre-initialized TLS vars (copied)
 *     +-------------------------+
 *     |      alignment padding  |
 *     +-------------------------+
 *     |      guard page         |  <- PROT_NONE
 *     +-------------------------+
 *     low address
 *
 * The initial thread's TLS block is set up by _start, before
 * main() is called. Additional threads' TLS blocks are set up by
 * pthread_create(), before the new thread runs.
 */

#ifndef _BITS_TLS_H
# define _BITS_TLS_H

# include <bits/types.h>
# include <bits/thread/tcb.h>

/* ----- Alignment ----- */
/**
 * The TLS block and the TCB must be aligned on a 16-byte boundary
 * to satisfy the ABI. On x86_64 with AVX, 32 or 64 bytes is
 * preferable, but 16 is the minimum required.
 */

# ifndef HAJ_TLS_ALIGN
#  define HAJ_TLS_ALIGN 64
# endif

/* ----- TLS image sizes ----- */
/**
 * These are filled by the linker. __haj_tls_tdata_size is the
 * size of the .tdata section (pre-initialized TLS variables).
 * __haj_tls_tbss_size is the size of the .tbss section
 * (zero-initialized TLS variables).
 *
 * The variables themselves are defined by the linker script or
 * by a small assembly file that uses the ALIGN / __tdata_start
 * symbols.
 */

extern size_t		__haj_tlsTdataSize;
extern size_t		__haj_tlsTbssSize;
extern size_t		__haj_tlsAlign;
extern const void	*__haj_tlsTdataStart;

/* ----- Initialization ----- */

/**
 * @brief Set up the TLS of the initial thread.
 *
 * Called by _start before main(). Allocates the TLS block for
 * the main thread, copies the .tdata section, zeroes .tbss,
 * initializes the TCB, and writes the TCB pointer into the
 * thread register (%fs / tpidr_el0).
 *
 * Returns 0 on success, -1 on failure.
 */
int __haj_tlsInit(void);

/**
 * @brief Allocate a thread stack.
 *
 * Allocates a stack of `size` bytes with a guard page of
 * `guard_size` bytes at the bottom. The guard page is marked
 * PROT_NONE, so a stack overflow triggers a SIGSEGV.
 *
 * @param size Size of the usable stack (in bytes).
 * @param guard_size Size of the guard page (in bytes).
 * @return Pointer to the base of the mapping (the guard page),
 *         or NULL on error.
 */
void *__haj_threadAllocStack(size_t size, size_t guard_size);

/**
 * @brief Free a TLS block allocated by __haj_tls_alloc().
 *
 * Unmaps the entire region: guard page, stack, TLS image, TCB.
 * @param tcb Pointer to the TCB of the thread to free.
 */
void __haj_tlsFree(struct __haj_tcb *tcb);

/**
 * @brief Copy the initial .tdata image into a new TLS block.
 *
 * Used by __haj_tls_alloc().
 * @param tlsBlock Pointer to the start of the TLS block (top of stack).
 */
void __haj_tlsCopyTdata(void *tlsBlock);

/* ----- Platform hooks ----- */

/**
 * @brief Write a TCB pointer into the thread register.
 *
 * On x86_64, uses arch_prctl(ARCH_SET_FS, tcb).
 * On aarch64, uses msr tpidr_el0, tcb.
 * On FreeBSD, uses amd64_set_fsbase / aarch64_set_tcb.
 * On Darwin, uses thread_set_self.
 */
int __haj_setThreadArea(struct __haj_tcb *tcb);

/**
 * @brief Read the TCB pointer from the thread register.
 *
 * On x86_64, uses arch_prctl(ARCH_GET_FS, &tcb).
 * On aarch64, uses mrs tcb, tpidr_el0.
 * On FreeBSD, uses amd64_get_fsbase / aarch64_get_tcb.
 * On Darwin, uses thread_get_self.
 */
void *__haj_threadAllocStack(size_t size, size_t guard_size);

/**
 * @brief Free a thread stack allocated by __haj_thread_alloc_stack().
 *
 * Unmaps the entire region: guard page, stack, TLS image, TCB.
 * @param base Pointer to the base of the mapping (guard page).
 * @param size Size of the stack (not including guard page).
 * @param guard_size Size of the guard page.
 */
void __haj_threadFreeStack(void *base, size_t size, size_t guard_size);

#endif /* _BITS_TLS_H */
