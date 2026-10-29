/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file tcb.c
 * @brief TLS areas and installation of the main thread's TCB.
 * @Created: 2026/10/02 08:53:32 by Moutig
 * @Updated: 2026/10/26 04:09:48 by Moutig
 *
 * Every thread owns a "TLS area": one contiguous, 64-byte aligned block
 * holding its TLS variables and its TCB. The linker script only
 * provides a TEMPLATE of the TLS variables (`.tdata` image + `.tbss`
 * size); it reserves no memory for any thread, the main one included
 * (`.tbss` occupies no address space, see mk/hajlibc.ld).
 *
 * x86_64 (TLS variant II, TCB at TP):
 *
 *	 high address
 *	 +---------------------+
 *	 |   TCB (struct)	  |  <-- TP == %fs == tcb, %fs:0 == tcb->self
 *	 +---------------------+
 *	 |   .tbss copy (zero) |
 *	 +---------------------+
 *	 |   .tdata copy	   |
 *	 +---------------------+  <-- area == TP - tlsMemSize
 *	 low address
 *
 * clang/ld emit `%fs:offset` with
 * `offset = var_address - (tls_start + tls_memsz)` (negative), so TP must
 * be exactly `area + tls_memsz`, with tls_memsz = __tbss_end -
 * __tdata_start (a multiple of 64, guaranteed by the linker script).
 *
 * aarch64 (TLS variant I, TLS above TP):
 *
 *	 +-----------------+-----------------+-------------------+
 *	 | TCB (span)	  | gap (16 -> al.) | TLS variables	 |
 *	 +-----------------+-----------------+-------------------+
 *	 ^ area			^ TP == tpidr_el0 ^ TP + gap
 *
 * ld/lld emit `tprel = var_address - tls_start + align_up(16, tls_align)`,
 * so the variables start `align_up(16, tls_align)` bytes after TP. The
 * TCB lives below TP; __haj_tcbSelf() steps back over it.
 */

#include <bits/crt.h>

#include <bits/thread/tcb.h>
#include <bits/syscall.h>
#include <bits/symbols.h>
#include <sys/auxv.h>
#include <string.h>

#if defined(__x86_64__)
# define ARCH_SET_FS	0x1002
#elif defined(__aarch64__)
/* No arch_prctl on aarch64; tpidr_el0 is written directly. */
#else
# error "Unsupported architecture in tcb.c"
#endif

/*
 * Code that runs before the thread register is installed must not read
 * the stack-protector canary (%fs:0x28 would be address 0x28 -> SIGSEGV).
 * Everything on the startup path below is therefore compiled without it.
 */
#if defined(__has_attribute)
# if __has_attribute(no_stack_protector)
#  define HAJ_NO_SSP	__attribute__((no_stack_protector))
# endif
#endif
#ifndef HAJ_NO_SSP
# define HAJ_NO_SSP
#endif

#define HAJ_TLS_ALIGN		64
#define HAJ_ALIGN_UP(x, a)	(((x) + ((size_t)(a) - 1)) & ~((size_t)(a) - 1))

#define HAJ_AT_NULL			0
#define HAJ_AT_RANDOM		25

/*
 * Upper bound for the main thread's TLS variables. Keep in sync with
 * the ASSERT at the end of the SECTIONS block in mk/hajlibc.ld.
 */
#define HAJ_MAIN_TLS_MAX	16384

/*
 * Pointer to the main thread's TCB, for code paths that don't have
 * access to the thread register. NULL until the main thread is
 * initialized.
 */
struct __haj_tcb *__haj_main_tcb_ptr;

/*
 * The main thread's TLS area, in .bss (zero-filled by the kernel).
 * Room for the TLS variables, the TCB and the aarch64 padding.
 */
static unsigned char	g_mainArea[HAJ_MAIN_TLS_MAX
								   + sizeof(struct __haj_tcb)
								   + 3 * HAJ_TLS_ALIGN]
						__attribute__((aligned(HAJ_TLS_ALIGN)));


/* ----- TLS area layout ----- */

static __HAJ_INLINE HAJ_NO_SSP size_t tlsMemSize(void)
{
		return ((size_t)(__tbss_end - __tdata_start));
}

#if defined(__aarch64__)
static __HAJ_INLINE HAJ_NO_SSP size_t tlsGap(void)
{
		return (HAJ_ALIGN_UP(16, (size_t)__tls_align));
}
#endif

HAJ_NO_SSP
size_t __haj_tlsAreaSize(void)
{
#if defined(__x86_64__)
		return (HAJ_ALIGN_UP(tlsMemSize() + sizeof(struct __haj_tcb), HAJ_TLS_ALIGN));
#else
		return (HAJ_ALIGN_UP(__HAJ_TCB_SPAN + tlsGap() + tlsMemSize(), HAJ_TLS_ALIGN));
#endif
}

HAJ_NO_SSP
struct __haj_tcb *__haj_tlsAreaInit(void *area)
{
		size_t				memsz = tlsMemSize();
		size_t				filesz = (size_t)(__tdata_end - __tdata_start);
		char				*blk;
		struct __haj_tcb	*tcb;

		/*
		 * Zero the whole area first: this takes care of .tbss, of the
		 * TCB, and of any padding in one go.
		 */
		memset(area, 0, __haj_tlsAreaSize());

#if defined(__x86_64__)
		blk = (char *)area;
		tcb = (struct __haj_tcb *)HAJ_ALIGN_UP((size_t)blk + memsz, HAJ_TLS_ALIGN);
#else
		tcb = (struct __haj_tcb *)area;
		blk = (char *)area + __HAJ_TCB_SPAN + tlsGap();
#endif

		/* Copy the .tdata template; .tbss is already zero. */
		memcpy(blk, __tdata_start, filesz);

		/*
		 * `self` must be at offset 0 because several code paths (and
		 * the ABI expectation for `%fs:0` on x86_64) read the TCB
		 * pointer from there.
		 */
		tcb->self	= tcb;
		tcb->selfId	= (pthread_t)tcb;
		tcb->state	= HAJ_THREAD_RUNNING;

		return (tcb);
}

/**
 * @brief Stack-protector canary for the main thread, from AT_RANDOM.
 *
 * The low byte is cleared (like glibc) so that a string overflow cannot
 * reproduce the canary. Written without arrays or address-taken locals
 * so that it never needs a canary itself.
 */
static HAJ_NO_SSP __haj_uintptr mainCanary(void)
{
		const unsigned long	*av = __haj_auxv;
		const unsigned char	*r;
		__haj_uintptr		c = 0;

		for (; av != NULL && av[0] != HAJ_AT_NULL; av += 2)
		{
				if (av[0] != HAJ_AT_RANDOM || av[1] == 0)
						continue;
				r = (const unsigned char *)av[1];
				for (size_t i = 0; i < sizeof(c); i++)
						c |= (__haj_uintptr)r[i] << (8 * i);
				break;
		}
		return (c & ~(__haj_uintptr)0xff);
}

HAJ_NO_SSP
void __hajSetThreadRegister(void)
{
		struct __haj_tcb	*tcb;
		void				*tp;

		if (__haj_main_tcb_ptr != NULL)
				return;

		/*
		 * Sanity checks, tied to mk/hajlibc.ld: the block alignment is
		 * the one tcb.c assumes, tls_memsz is a multiple of it, and
		 * everything fits in the static area. Anything else means the
		 * linker script and this file are out of sync.
		 */
		if ((size_t)__tls_align > HAJ_TLS_ALIGN
			|| (tlsMemSize() & (HAJ_TLS_ALIGN - 1)) != 0
			|| __haj_tlsAreaSize() > sizeof(g_mainArea))
				__builtin_trap();

		/*
		 * 1. Build the main thread's TLS area (template copy + TCB).
		 */
		tcb = __haj_tlsAreaInit(g_mainArea);

		/*
		 * 2. Final canary value, set BEFORE the thread register is
		 *	installed so that no function ever sees it change.
		 */
		tcb->stackCanary = mainCanary();

		/*
		 * 3. Publish the TCB pointer for code paths that don't have
		 *	access to the thread register.
		 */
		__haj_main_tcb_ptr = tcb;

		/*
		 * 4. Install the thread register.
		 *
		 * x86_64:  %fs  = TP = tcb (TLS variables are at negative
		 *		  offsets from it, `%fs:0` is tcb->self).
		 * aarch64: tpidr_el0 = TP = tcb + span (TLS variables are at
		 *		  positive offsets from it).
		 */
		tp = __haj_tcbToTp(tcb);
#if defined(__x86_64__)
		if (__haj_syscall2(SYS_arch_prctl, ARCH_SET_FS, (long)tp) != 0)
			__builtin_trap();
#elif defined(__aarch64__)
		__asm__ volatile ("msr tpidr_el0, %0" :: "r" (tp) : "memory");
#endif

#if HAJ_PTHREAD_PROCESS_SHARED
		/*
		 * Register the robust-list head with the kernel. This is a
		 * no-op on the main thread unless the process explicitly
		 * opted into process-shared pthread objects.
		 */
		__haj_robustInit(&tcb->robustList);
#endif
}
