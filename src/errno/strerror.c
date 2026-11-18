/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlibc.
 * See LICENSE for the full license text.
 */

/**
 * @file strerror.c
 * @brief Translate an error code to a human-readable string.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/10/05 15:48:34 by Moutig
 *
 * For known codes, strerror() returns a pointer to a static,
 * read-only string. Those strings live in .rodata and are
 * shared by every thread without synchronization, so the call
 * is trivially thread-safe.
 *
 * For unknown codes, strerror() formats "Unknown error N" into
 * a thread-local buffer and returns a pointer to it. The
 * buffer is overwritten on each call from the same thread, so
 * callers that need to keep the result must copy it. This
 * matches the behaviour of the glibc and musl implementations.
 *
 * The error codes are per-OS. The strings are the standard
 * descriptions from the POSIX specification, the Linux man
 * pages, the FreeBSD man pages, and the macOS man pages.
 *
 * Implementation notes:
 *
 *   - Codes are looked up in a sparse table indexed by errno
 *     value, not by a switch. The table is bounded (see
 *     HAJ_STRERROR_MAX), so anything above the bound falls
 *     through to the unknown-code path.
 *
 *   - The table is read-only and lives in .rodata. There is
 *     no initialization code and no lock.
 *
 *   - The unknown-code buffer is __HAJ_THREAD_LOCAL, so each
 *     thread gets its own copy.
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <bits/compiler.h>
#include <bits/os.h>

/* ----- Thread-local buffer for unknown error codes ----- */

/*
 * 64 bytes is enough for "Unknown error " (14) + 20 digits
 * (INT64_MIN) + NUL. Anything longer would indicate a bug in
 * the formatter, not in the caller's errno value.
 */
static __HAJ_THREAD_LOCAL char	g_strerrorBuf[64];

/*
 * Upper bound of the table. Codes above this fall through to
 * the unknown-code path. 200 covers every POSIX and Linux
 * code, plus the FreeBSD and Darwin extensions.
 */
# define HAJ_STRERROR_MAX	200

/* ----- Error string table ----- */

/*
 * Sparse table: entries that are 0 (NULL) mean "no description
 * for this code on this platform". Duplicate codes from the
 * per-OS sections are merged: we keep the first non-NULL.
 *
 * On Linux, the values of EDEADLK and EDEADLOCK are the same,
 * so only one entry is needed. On FreeBSD and Darwin,
 * EDEADLOCK is not defined, and the entry is left NULL.
 *
 * The order of the initializers does not matter: designated
 * initializers assign to the right slot, and later duplicates
 * override earlier ones. We rely on that to let the per-OS
 * block fill in the OS-specific codes after the common ones.
 */
static const char *const	g_errStrings[HAJ_STRERROR_MAX] = {
	/* ----- Generic codes, same value on all Unix-like OSes ----- */
	[0]					= "Success",
	[EPERM]				= "Operation not permitted",
	[ENOENT]			= "No such file or directory",
	[ESRCH]				= "No such process",
	[EINTR]				= "Interrupted system call",
	[EIO]				= "Input/output error",
	[ENXIO]				= "No such device or address",
	[E2BIG]				= "Argument list too long",
	[ENOEXEC]			= "Exec format error",
	[EBADF]				= "Bad file descriptor",
	[ECHILD]			= "No child processes",
	[ENOMEM]			= "Cannot allocate memory",
	[EACCES]			= "Permission denied",
	[EFAULT]			= "Bad address",
	[ENOTBLK]			= "Block device required",
	[EBUSY]				= "Device or resource busy",
	[EEXIST]			= "File exists",
	[EXDEV]				= "Invalid cross-device link",
	[ENODEV]			= "No such device",
	[ENOTDIR]			= "Not a directory",
	[EISDIR]			= "Is a directory",
	[EINVAL]			= "Invalid argument",
	[ENFILE]			= "Too many open files in system",
	[EMFILE]			= "Too many open files",
	[ENOTTY]			= "Inappropriate ioctl for device",
	[ETXTBSY]			= "Text file busy",
	[EFBIG]				= "File too large",
	[ENOSPC]			= "No space left on device",
	[ESPIPE]			= "Illegal seek",
	[EROFS]				= "Read-only file system",
	[EMLINK]			= "Too many links",
	[EPIPE]				= "Broken pipe",
	[EDOM]				= "Numerical argument out of domain",
	[ERANGE]			= "Numerical result out of range",
	[EDEADLK]			= "Resource deadlock avoided",
	[ENAMETOOLONG]		= "File name too long",
	[ENOLCK]			= "No locks available",
	[ENOSYS]			= "Function not implemented",
	[ENOTEMPTY]			= "Directory not empty",
	[ELOOP]				= "Too many levels of symbolic links",

	/* ----- Networking codes, common to all Unix-like OSes ----- */
	[ENOTSOCK]			= "Socket operation on non-socket",
	[EDESTADDRREQ]		= "Destination address required",
	[EMSGSIZE]			= "Message too long",
	[EPROTOTYPE]		= "Protocol wrong type for socket",
	[ENOPROTOOPT]		= "Protocol not available",
	[EPROTONOSUPPORT]	= "Protocol not supported",
	[ESOCKTNOSUPPORT]	= "Socket type not supported",
	[EOPNOTSUPP]		= "Operation not supported",
	[EPFNOSUPPORT]		= "Protocol family not supported",
	[EAFNOSUPPORT]		= "Address family not supported by protocol",
	[EADDRINUSE]		= "Address already in use",
	[EADDRNOTAVAIL]		= "Cannot assign requested address",
	[ENETDOWN]			= "Network is down",
	[ENETUNREACH]		= "Network is unreachable",
	[ENETRESET]			= "Network dropped connection on reset",
	[ECONNABORTED]		= "Software caused connection abort",
	[ECONNRESET]		= "Connection reset by peer",
	[ENOBUFS]			= "No buffer space available",
	[EISCONN]			= "Transport endpoint is already connected",
	[ENOTCONN]			= "Transport endpoint is not connected",
	[ESHUTDOWN]			= "Cannot send after transport endpoint shutdown",
	[ETOOMANYREFS]		= "Too many references: cannot splice",
	[ETIMEDOUT]			= "Connection timed out",
	[ECONNREFUSED]		= "Connection refused",
	[EHOSTDOWN]			= "Host is down",
	[EHOSTUNREACH]		= "No route to host",
	[EALREADY]			= "Operation already in progress",
	[EINPROGRESS]		= "Operation now in progress",
	[ESTALE]			= "Stale file handle",

	/* ----- POSIX realtime / message queue codes ----- */
	[ENOMSG]			= "No message of desired type",
	[EIDRM]				= "Identifier removed",
	[EPROTO]			= "Protocol error",
	[EMULTIHOP]			= "Multihop attempted",
	[EBADMSG]			= "Bad message",
	[EOVERFLOW]			= "Value too large for defined data type",
	[EILSEQ]			= "Invalid or incomplete multibyte sequence",
	[ECANCELED]			= "Operation canceled",

# if defined(HAJ_OS_LINUX)

	[ENOSTR]			= "Device not a stream",
	[ENODATA]			= "No data available",
	[ETIME]				= "Timer expired",
	[ENOSR]				= "Out of streams resources",
	[ENONET]			= "Machine is not on the network",
	[EREMOTE]			= "Object is remote",
	[ENOLINK]			= "Link has been severed",
	[EADV]				= "Advertise error",
	[ESRMNT]			= "Srmount error",
	[ECOMM]				= "Communication error on send",
	[EDOTDOT]			= "RFS specific error",
	[ENOTUNIQ]			= "Name not unique on network",
	[EBADFD]			= "File descriptor in bad state",
	[EREMCHG]			= "Remote address changed",
	[ELIBACC]			= "Cannot access a needed shared library",
	[ELIBBAD]			= "Accessing a corrupted shared library",
	[ELIBSCN]			= ".lib section in a.out corrupted",
	[ELIBMAX]			= "Attempting to link in too many shared libraries",
	[ELIBEXEC]			= "Cannot exec a shared library directly",
	[ERESTART]			= "Interrupted system call should be restarted",
	[ESTRPIPE]			= "Streams pipe error",
	[EUSERS]			= "Too many users",
	[EDQUOT]			= "Disk quota exceeded",
	[EOWNERDEAD]		= "Owner died",
	[ENOTRECOVERABLE]	= "State not recoverable",

# elif defined(HAJ_OS_FREEBSD)

	[EPROCLIM]			= "Too many processes",
	[EUSERS]			= "Too many users",
	[EDQUOT]			= "Disc quota exceeded",
	[EBADRPC]			= "RPC struct is bad",
	[ERPCMISMATCH]		= "RPC version wrong",
	[EPROGUNAVAIL]		= "RPC prog. not avail",
	[EPROGMISMATCH]		= "Program version wrong",
	[EPROCUNAVAIL]		= "Bad procedure for program",
	[EFTYPE]			= "Inappropriate file type or format",
	[EAUTH]				= "Authentication error",
	[ENEEDAUTH]			= "Need authenticator",
	[ENOATTR]			= "Attribute not found",
	[ENOTCAPABLE]		= "Capabilities insufficient",

# elif defined(HAJ_OS_DARWIN)

	[EPROCLIM]			= "Too many processes",
	[EUSERS]			= "Too many users",
	[EDQUOT]			= "Disc quota exceeded",
	[EBADRPC]			= "RPC struct is bad",
	[ERPCMISMATCH]		= "RPC version wrong",
	[EPROGUNAVAIL]		= "RPC prog. not avail",
	[EPROGMISMATCH]		= "Program version wrong",
	[EPROCUNAVAIL]		= "Bad procedure for program",
	[EFTYPE]			= "Inappropriate file type or format",
	[EAUTH]				= "Authentication error",
	[ENEEDAUTH]			= "Need authenticator",
	[EPWROFF]			= "Device power is off",
	[EDEVERR]			= "Device error",
	[EBADEXEC]			= "Bad executable (or shared library)",
	[EBADARCH]			= "Bad CPU type in executable",
	[ESHLIBVERS]		= "Shared library version mismatch",
	[EBADMACHO]			= "Malformed Mach-o file",
	[ENOATTR]			= "Attribute not found",
	[ENOPOLICY]			= "No such policy registered",
	[ENOTRECOVERABLE]	= "State not recoverable",
	[EOWNERDEAD]		= "Previous owner died",
	[EQFULL]			= "Interface output queue is full",
	[ENODATA]			= "No data available",
	[ENOSR]				= "Out of streams resources",
	[ENOSTR]			= "Device not a stream",
	[ETIME]				= "Timer expired",
	[ENOLINK]			= "Link has been severed",

# elif defined(HAJ_OS_WINDOWS)

	/*
	 * Windows CRT errno values. Most are shared with Unix,
	 * but a few are Windows-specific.
	 */
	[EDEADLOCK]			= "Resource deadlock avoided",
	[STRUNCATE]			= "String was truncated",

# endif
};


/* ----- Unknown-code formatter ----- */

static size_t	formatUnknown(int errnum, char *buf, size_t len)
{
	static const char	prefix[] = "Unknown error ";
	size_t				pos = 0;
	char				digits[24];
	int					ndigits = 0;
	int					negative;
	unsigned int		u;

	/* Copy the prefix, leaving room for the NUL. */
	for (size_t i = 0; i < sizeof(prefix) - 1 && pos + 1 < len; i++)
		buf[pos++] = prefix[i];

	negative = (errnum < 0);
	u = negative ? (unsigned int)(-(long)errnum)
				 : (unsigned int)errnum;

	if (u == 0) {
		digits[ndigits++] = '0';
	} else {
		while (u > 0 && ndigits < (int)sizeof(digits)) {
			digits[ndigits++] = (char)('0' + (u % 10));
			u /= 10;
		}
	}

	if (negative && pos + 1 < len)
		buf[pos++] = '-';

	while (ndigits > 0 && pos + 1 < len)
		buf[pos++] = digits[--ndigits];

	buf[pos] = '\0';
	return (pos);
}


/* ----- strerror ----- */

char *strerror(int errnum)
{
	const char	*msg;

	if (errnum >= 0 && errnum < HAJ_STRERROR_MAX
		&& (msg = g_errStrings[errnum]) != NULL)
		return ((char *)msg);

	formatUnknown(errnum, g_strerrorBuf, sizeof(g_strerrorBuf));
	return (g_strerrorBuf);
}


/* ----- strerror_r ----- */

int strerror_r(int errnum, char *buf, size_t buflen)
{
	const char	*msg;
	size_t		len;

	if (buf == NULL || buflen == 0)
		return (EINVAL);

	if (errnum >= 0 && errnum < HAJ_STRERROR_MAX
		&& (msg = g_errStrings[errnum]) != NULL) {
		len = strlen(msg);
		if (len + 1 > buflen) {
			memcpy(buf, msg, buflen - 1);
			buf[buflen - 1] = '\0';
			return (ERANGE);
		}
		memcpy(buf, msg, len + 1);
		return (0);
	}

	len = formatUnknown(errnum, buf, buflen);
	if (len + 1 > buflen)
		return (ERANGE);
	return (0);
}
