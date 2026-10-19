/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@student.42lehavre.fr>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file message.c
 * @brief Message formatting and output for hajmalloc.
 * @Created: 2026/10/06 12:37:31 by Moutig
 * @Updated: 2026/10/15 01:16:57 by Moutig
 *
 * The allocator must be able to report errors, warnings and
 * verbose information without calling the libc's stdio.
 * printf and friends may call malloc, which would recurse into
 * the very code that is trying to report the problem.
 *
 * This file provides:
 *
 *   - a minimal formatter, hmallocFmt, that understands the
 *     handful of conversions the allocator uses,
 *     and EINTR,
 *   - an integer-to-ASCII converter, hmallocUtoa,
 *   - the entry points used by the rest of the allocator:
 *     _hmallocErrorMessage, _hmallocWarningMessage,
 *     _hmallocVerboseMessage, _hmallocAssertFail,
 *   - the three callback registration functions from the
 *     public header.
 */

#include "include/hmInternal.h"
#include "include/hmPrim.h"
#include "../../../include/malloc.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>


size_t	hmallocUtoa(char *buf, size_t cap, uint64_t v,
				   unsigned base, bool upper)
{
	static const char	lower[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	static const char	upperd[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	const char			*digits;
	char				tmp[64];
	size_t				n;
	size_t				i;

	if (buf == NULL || cap == 0)
		return (0);
	if (base < 2 || base > 36)
		base = 10;
	digits = upper ? upperd : lower;

	n = 0;
	if (v == 0) {
		if (n + 1 < cap)
			tmp[n++] = '0';
	} else {
		while (v != 0 && n + 1 < cap) {
			tmp[n++] = digits[v % base];
			v /= base;
		}
	}

	for (i = 0; i < n; i++)
		buf[i] = tmp[n - 1 - i];
	buf[n] = '\0';
	return (n);
}

/* ----- Formatter ----- */

/*
 * The formatter understands the following conversions:
 *
 *   %s   NUL-terminated string
 *   %c   single character
 *   %d   signed int
 *   %u   unsigned int
 *   %zu  size_t
 *   %lu  unsigned long
 *   %ld  signed long
 *   %x   lowercase hexadecimal, unsigned int
 *   %lX  uppercase hexadecimal, unsigned long
 *   %p   pointer, printed as 0x followed by uppercase hex
 *   %%   literal percent
 *
 * Width and precision are not supported. Long and long long
 * modifiers are only supported as part of the conversions
 * listed above.
 */

/**
 * @brief Advance the destination pointer and update the count.
 *
 * Helper used by hmallocFmt to write a single character while
 * respecting the buffer capacity. When the buffer is full,
 * the function stops writing but continues to increment the
 * would-be length, so the caller can detect truncation.
 */
struct hmFmtState {
	char	*buf;
	size_t	cap;
	size_t	pos;
};

static void	hmFmtPutc(struct hmFmtState *st, char c)
{
	if (st->pos + 1 < st->cap)
		st->buf[st->pos] = c;
	st->pos++;
}

static void	hmFmtPuts(struct hmFmtState *st, const char *s)
{
	if (s == NULL)
		s = "(null)";
	while (*s != '\0')
		hmFmtPutc(st, *s++);
}

static void	hmFmtPutn(struct hmFmtState *st, const char *s, size_t n)
{
	for (size_t i = 0; i < n; i++)
		hmFmtPutc(st, s[i]);
}

static void	hmFmtPutUnsigned(struct hmFmtState *st, uint64_t v,
								 unsigned base, bool upper)
{
	char	buf[64];
	size_t	n;

	n = hmallocUtoa(buf, sizeof(buf), v, base, upper);
	hmFmtPutn(st, buf, n);
}

static void	hmFmtPutSigned(struct hmFmtState *st, int64_t v)
{
	if (v < 0) {
		hmFmtPutc(st, '-');
		/*
		 * -INT64_MIN is undefined behavior; use the
		 * standard trick of computing (v + 1) as a
		 * negative number first, then adding one back
		 * after the negation.
		 */
		hmFmtPutUnsigned(st,
						 (uint64_t)(-(v + 1)) + 1ULL,
						 10, false);
	} else {
		hmFmtPutUnsigned(st, (uint64_t)v, 10, false);
	}
}

size_t	hmallocFmt(char *buf, size_t cap, const char *fmt, va_list ap)
{
	struct hmFmtState	st;

	st.buf = buf;
	st.cap = cap;
	st.pos = 0;

	if (buf == NULL || cap == 0 || fmt == NULL)
		return (0);

	while (*fmt != '\0') {
		if (*fmt != '%') {
			hmFmtPutc(&st, *fmt++);
			continue;
		}

		fmt++;	/* skip '%' */

		/*
		 * Long modifiers. Only "l" and "ll" are used in the
		 * allocator, so a single lookahead is enough.
		 */
		int	isLong = 0;
		int	isLongLong = 0;
		if (*fmt == 'l') {
			isLong = 1;
			fmt++;
			if (*fmt == 'l') {
				isLongLong = 1;
				fmt++;
			}
		} else if (*fmt == 'z') {
			isLong = 2;	/* z is size_t, same width as long on LP64 */
			fmt++;
		}

		switch (*fmt) {
		case 's':
			hmFmtPuts(&st, va_arg(ap, const char *));
			break;
		case 'c':
			hmFmtPutc(&st, (char)va_arg(ap, int));
			break;
		case 'd': {
			int64_t	v;
			if (isLongLong)
				v = va_arg(ap, long long);
			else if (isLong == 1)
				v = va_arg(ap, long);
			else if (isLong == 2)
				v = (int64_t)va_arg(ap, size_t);
			else
				v = va_arg(ap, int);
			hmFmtPutSigned(&st, v);
			break ;
		}
		case 'u': {
			uint64_t	v;
			if (isLongLong)
				v = va_arg(ap, unsigned long long);
			else if (isLong == 1)
				v = va_arg(ap, unsigned long);
			else if (isLong == 2)
				v = va_arg(ap, size_t);
			else
				v = va_arg(ap, unsigned int);
			hmFmtPutUnsigned(&st, v, 10, false);
			break ;
		}
		case 'x': {
			uint64_t	v;
			if (isLongLong)
				v = va_arg(ap, unsigned long long);
			else if (isLong == 1)
				v = va_arg(ap, unsigned long);
			else if (isLong == 2)
				v = va_arg(ap, size_t);
			else
				v = va_arg(ap, unsigned int);
			hmFmtPutUnsigned(&st, v, 16, false);
			break ;
		}
		case 'X': {
			uint64_t	v;
			if (isLongLong)
				v = va_arg(ap, unsigned long long);
			else if (isLong == 1)
				v = va_arg(ap, unsigned long);
			else if (isLong == 2)
				v = va_arg(ap, size_t);
			else
				v = va_arg(ap, unsigned int);
			hmFmtPutUnsigned(&st, v, 16, true);
			break ;
		}
		case 'p': {
			void	*p = va_arg(ap, void *);

			hmFmtPutc(&st, '0');
			hmFmtPutc(&st, 'x');
			hmFmtPutUnsigned(&st, (uint64_t)(uintptr_t)p,
							 16, true);
			break ;
		}
		case '%':
			hmFmtPutc(&st, '%');
			break ;
		case '\0':
			/*
			 * Trailing % at the end of the format.
			 * Print it literally and stop.
			 */
			hmFmtPutc(&st, '%');
			fmt--;
			break ;
		default:
			/*
			 * Unknown conversion. Print it verbatim,
			 * prefixed by a percent sign, so the
			 * caller can see what went wrong.
			 */
			hmFmtPutc(&st, '%');
			hmFmtPutc(&st, *fmt);
			break ;
		}
		fmt++;
	}

	/* NUL-terminate, even when truncated. */
	if (st.pos < st.cap)
		st.buf[st.pos] = '\0';
	else
		st.buf[st.cap - 1] = '\0';

	return (st.pos);
}

static void	hmDefaultOutput(const char *msg, void *arg)
{
	(void)arg;
	hmallocPrimOutStderr(msg);
}

/**
 * @brief Emit a formatted message through the output callback.
 *
 * Formats into a 512-byte stack buffer. If the message is
 * longer, it is truncated. The formatted text is then passed
 * to the output callback, or to the default writer if no
 * callback is registered.
 *
 * @param fmt Format string.
 * @param ap  Variadic argument list.
 */
static void	hmEmit(const char *fmt, va_list ap)
{
	char				buf[512];
	t_hmOutputFun		*fn;
	void				*arg;

	hmallocFmt(buf, sizeof(buf), fmt, ap);

	fn = g_hmalloc.outFun;
	arg = g_hmalloc.outArg;
	if (fn == NULL)
		hmDefaultOutput(buf, arg);
	else
		fn(buf, arg);
}

void	_hmallocErrorMessage(int err, const char *fmt, ...)
{
	va_list	ap;

	/*
	 * If an error callback is registered, it takes the
	 * message. It receives the errno code and its own
	 * argument, and is responsible for any formatting.
	 */
	if (g_hmalloc.errFun != NULL) {
		g_hmalloc.errFun(err, g_hmalloc.errArg);
		return ;
	}

	if (g_hmalloc.options[HMALLOC_OPT_SHOW_ERRORS] == 0)
		return ;

	va_start(ap, fmt);
	hmEmit(fmt, ap);
	va_end(ap);
}

void	_hmallocWarningMessage(const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	hmEmit(fmt, ap);
	va_end(ap);
}

void	_hmallocVerboseMessage(const char *fmt, ...)
{
	va_list	ap;

	if (g_hmalloc.options[HMALLOC_OPT_VERBOSE] == 0)
		return ;

	va_start(ap, fmt);
	hmEmit(fmt, ap);
	va_end(ap);
}

_hmDeclNoreturn void	_hmallocAssertFail(const char *expr,
										 const char *file,
										 unsigned line,
										 const char *func)
{
	char		buf[512];
	char		num[32];
	size_t		pos;
	const char	*parts[4];
	size_t		n;

	/*
	 * Build the message by hand. Each piece is copied
	 * literally; hmallocFmt is not used because the
	 * pieces are already strings and numbers formatted by
	 * hmallocUtoa.
	 */
	pos = 0;
	parts[0] = "hmalloc: assertion failed: ";
	parts[1] = expr;
	parts[2] = " at ";
	parts[3] = file;

	for (int i = 0; i < 4; i++) {
		const char	*p = parts[i];
		while (*p != '\0' && pos + 1 < sizeof(buf))
			buf[pos++] = *p++;
	}

	n = hmallocUtoa(num, sizeof(num), (uint64_t)line, 10, false);
	if (pos + 1 < sizeof(buf))
		buf[pos++] = ':';
	for (size_t i = 0; i < n && pos + 1 < sizeof(buf); i++)
		buf[pos++] = num[i];

	if (pos + 3 < sizeof(buf)) {
		buf[pos++] = ' ';
		buf[pos++] = '(';
		while (*func != '\0' && pos + 1 < sizeof(buf))
			buf[pos++] = *func++;
		buf[pos++] = ')';
	}

	buf[pos] = '\0';

	hmallocPrimOutStderr(buf);
	abort();
}

void	hmRegisterOutput(t_hmOutputFun *out, void *arg)
{
	g_hmalloc.outFun = out;
	g_hmalloc.outArg = arg;
}

void	hmRegisterError(t_hmErrorFun *fun, void *arg)
{
	g_hmalloc.errFun = fun;
	g_hmalloc.errArg = arg;
}

void	hmRegisterDeferredFree(t_hmDeferredFreeFun *fn, void *arg)
{
	g_hmalloc.deferredFun = fn;
	g_hmalloc.deferredArg = arg;
}
