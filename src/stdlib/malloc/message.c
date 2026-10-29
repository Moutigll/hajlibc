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
 * @Updated: 2026/10/29 05:03:28 by Moutig
 *
 * The allocator must be able to report errors, warnings and
 * verbose information without calling the libc's stdio.
 * printf and friends may call malloc, which would recurse into
 * the very code that is trying to report the problem.
 *
 * This file provides:
 *
 *   - a minimal formatter, hmallocFmt / hmallocFmtInto, that
 *     understands the subset of printf used by the allocator:
 *       %[-][0][width][l|ll|z](s|c|d|u|x|X|p|%)
 *     with '-' (left-justify), '0' (zero-pad), width,
 *     and %p as 0x + 8 or 16 hex digits zero-padded lowercase.
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

#include <stdlib.h>


size_t	hmallocUtoa(char *buf, size_t cap, uint64_t v, unsigned base, bool upper)
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

static void	hmFmtPad(struct hmFmtState *st, size_t n, char c)
{
	while (n--)
		hmFmtPutc(st, c);
}

static void	hmFmtPutUnsignedW(struct hmFmtState *st, uint64_t v,
							  unsigned base, bool upper,
							  int width, int left, int zero)
{
	char	buf[64];
	size_t	n = hmallocUtoa(buf, sizeof(buf), v, base, upper);
	size_t	pad = (width > (int)n) ? (size_t)width - n : 0;

	if (!left)
		hmFmtPad(st, pad, zero ? '0' : ' ');
	hmFmtPutn(st, buf, n);
	if (left)
		hmFmtPad(st, pad, ' ');
}

/* Signed, with width / left-justify / zero-pad (sign before zeros). */
static void	hmFmtPutSignedW(struct hmFmtState *st, int64_t v, int width, int left, int zero)
{
	char		buf[64];
	uint64_t	uval;
	size_t		n, total, pad;
	int			neg = (v < 0);

	if (neg)
		uval = (uint64_t)(-(v + 1)) + 1;
	else
		uval = (uint64_t)v;
	n = hmallocUtoa(buf, sizeof(buf), uval, 10, false);
	total = n + (neg ? 1 : 0);
	pad = (width > (int)total) ? (size_t)width - total : 0;

	if (left) {
		if (neg)
			hmFmtPutc(st, '-');
		hmFmtPutn(st, buf, n);
		hmFmtPad(st, pad, ' ');
	} else if (zero) {
		if (neg)
			hmFmtPutc(st, '-');
		hmFmtPad(st, pad, '0');
		hmFmtPutn(st, buf, n);
	} else {
		hmFmtPad(st, pad, ' ');
		if (neg)
			hmFmtPutc(st, '-');
		hmFmtPutn(st, buf, n);
	}
}

static void	hmFmtPutAddr(struct hmFmtState *st, uintptr_t v)
{
	static const char	hx[] = "0123456789abcdef";
	int					digits = (v <= 0xFFFFFFFFUL) ? 8 : 16;

	hmFmtPutc(st, '0');
	hmFmtPutc(st, 'x');
	for (int i = digits - 1; i >= 0; i--)
		hmFmtPutc(st, hx[(v >> (i * 4)) & 0xF]);
}

void	hmallocFmtInto(struct hmFmtState *st, const char *fmt, va_list ap)
{
	int	left, zero, width;
	int	isLong, isLongLong;

	if (st == NULL || fmt == NULL)
		return;

	while (*fmt != '\0') {
		if (*fmt != '%') {
			hmFmtPutc(st, *fmt++);
			continue;
		}

		fmt++;	/* skip '%' */

		/* Flags. */
		left = 0;
		zero = 0;
		for (;;) {
			if (*fmt == '-') { left = 1; fmt++; }
			else if (*fmt == '0') { zero = 1; fmt++; }
			else break;
		}

		/* Width. */
		width = 0;
		while (*fmt >= '0' && *fmt <= '9') {
			width = width * 10 + (*fmt - '0');
			fmt++;
		}

		/*
		 * Long modifiers. Only "l" and "ll" are used in the
		 * allocator, so a single lookahead is enough.
		 */
		isLong = 0;
		isLongLong = 0;
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
		case 's': {
			const char	*s = va_arg(ap, const char *);
			size_t		len = 0, pad;

			if (s == NULL)
				s = "(null)";
			while (s[len] != '\0')
				len++;
			pad = (width > (int)len) ? (size_t)width - len : 0;
			if (!left)
				hmFmtPad(st, pad, ' ');
			hmFmtPuts(st, s);
			if (left)
				hmFmtPad(st, pad, ' ');
			break;
		}
		case 'c': {
			char	c = (char)va_arg(ap, int);
			size_t	pad = (width > 1) ? (size_t)width - 1 : 0;

			if (!left)
				hmFmtPad(st, pad, ' ');
			hmFmtPutc(st, c);
			if (left)
				hmFmtPad(st, pad, ' ');
			break;
		}
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
			hmFmtPutSignedW(st, v, width, left, zero);
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
			hmFmtPutUnsignedW(st, v, 10, false, width, left, zero);
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
			hmFmtPutUnsignedW(st, v, 16, false, width, left, zero);
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
			hmFmtPutUnsignedW(st, v, 16, true, width, left, zero);
			break ;
		}
		case 'p': {
			void	*p = va_arg(ap, void *);

			hmFmtPutAddr(st, (uintptr_t)p);
			break ;
		}
		case '%':
			hmFmtPutc(st, '%');
			break ;
		case '\0':
			/*
			 * Trailing % at the end of the format.
			 * Print it literally and stop.
			 */
			hmFmtPutc(st, '%');
			fmt--;
			break ;
		default:
			/*
			 * Unknown conversion. Print it verbatim,
			 * prefixed by a percent sign, so the
			 * caller can see what went wrong.
			 */
			hmFmtPutc(st, '%');
			hmFmtPutc(st, *fmt);
			break ;
		}
		fmt++;
	}
}

size_t	hmallocFmt(char *buf, size_t cap, const char *fmt, va_list ap)
{
	struct hmFmtState	st;

	if (buf == NULL || cap == 0 || fmt == NULL)
		return (0);

	st.buf = buf;
	st.cap = cap;
	st.pos = 0;

	hmallocFmtInto(&st, fmt, ap);

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

#if HMALLOC_DEBUG > 0

_hmDeclNoreturn _hmDeclHidden void
_hmallocAssertFail(const char *expr, const char *file, unsigned line, const char *func)
{
	_hmallocErrorMessage(EINVAL,
		"hmalloc: assertion failed: %s\n"
		"  at %s:%u in %s()\n",
		expr ? expr : "(null)",
		file ? file : "?",
		line,
		func ? func : "?");
	abort();
}

#endif /* HMALLOC_DEBUG > 0 */

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
