/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file strerror.c
 * @brief Translate an error code to a human-readable string.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 18:28:44 by Moutig
 *
 * This implementation returns a pointer to a static, read-only
 * string. It is therefore safe to call from multiple threads
 * concurrently, as long as the caller does not modify the buffer.
 *
 * The error codes are per-OS. The strings are the standard
 * descriptions from the POSIX specification, the Linux man pages,
 * the FreeBSD man pages, and the macOS man pages.
 *
 * For unknown codes, we return "Unknown error N" using a
 * thread-local buffer. This buffer is overwritten on each call
 * from the same thread, so callers must copy the result if they
 * need to keep it.
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>

/* --------------------------------------------------------------------------
 * Thread-local buffer for unknown error codes
 * --------------------------------------------------------------------------
 *
 * When we don't know the code, we format "Unknown error N" into
 * this buffer. It is thread-local, so each thread has its own.
 */

# if defined(_MSC_VER)
#  define __HAJ_TLS	__declspec(thread)
# elif defined(__GNUC__) || defined(__clang__)
#  define __HAJ_TLS	__thread
# else
#  define __HAJ_TLS
# endif

static __HAJ_TLS char	__haj_strerror_buf[64];

const char *strerror(int errnum)
{
	switch (errnum) {
	/* ----- Generic codes, same value on all Unix-like OSes ----- */
	case 0:				return ("Success");
	case EPERM:			return ("Operation not permitted");
	case ENOENT:		return ("No such file or directory");
	case ESRCH:			return ("No such process");
	case EINTR:			return ("Interrupted system call");
	case EIO:			return ("Input/output error");
	case ENXIO:			return ("No such device or address");
	case E2BIG:			return ("Argument list too long");
	case ENOEXEC:		return ("Exec format error");
	case EBADF:			return ("Bad file descriptor");
	case ECHILD:		return ("No child processes");
	case ENOMEM:		return ("Cannot allocate memory");
	case EACCES:		return ("Permission denied");
	case EFAULT:		return ("Bad address");
	case ENOTBLK:		return ("Block device required");
	case EBUSY:			return ("Device or resource busy");
	case EEXIST:		return ("File exists");
	case EXDEV:			return ("Invalid cross-device link");
	case ENODEV:		return ("No such device");
	case ENOTDIR:		return ("Not a directory");
	case EISDIR:		return ("Is a directory");
	case EINVAL:		return ("Invalid argument");
	case ENFILE:		return ("Too many open files in system");
	case EMFILE:		return ("Too many open files");
	case ENOTTY:		return ("Inappropriate ioctl for device");
	case ETXTBSY:		return ("Text file busy");
	case EFBIG:			return ("File too large");
	case ENOSPC:		return ("No space left on device");
	case ESPIPE:		return ("Illegal seek");
	case EROFS:			return ("Read-only file system");
	case EMLINK:		return ("Too many links");
	case EPIPE:			return ("Broken pipe");
	case EDOM:			return ("Numerical argument out of domain");
	case ERANGE:		return ("Numerical result out of range");
	case EDEADLK:		return ("Resource deadlock avoided");
	case ENAMETOOLONG:	return ("File name too long");
	case ENOLCK:		return ("No locks available");
	case ENOSYS:		return ("Function not implemented");
	case ENOTEMPTY:		return ("Directory not empty");
	case ELOOP:			return ("Too many levels of symbolic links");

	/* ----- Networking codes, common to all Unix-like OSes ----- */
	case ENOTSOCK:		return ("Socket operation on non-socket");
	case EDESTADDRREQ:	return ("Destination address required");
	case EMSGSIZE:		return ("Message too long");
	case EPROTOTYPE:	return ("Protocol wrong type for socket");
	case ENOPROTOOPT:	return ("Protocol not available");
	case EPROTONOSUPPORT: return ("Protocol not supported");
	case ESOCKTNOSUPPORT: return ("Socket type not supported");
	case EOPNOTSUPP:	return ("Operation not supported");
	case EPFNOSUPPORT:	return ("Protocol family not supported");
	case EAFNOSUPPORT:	return ("Address family not supported by protocol");
	case EADDRINUSE:	return ("Address already in use");
	case EADDRNOTAVAIL:	return ("Cannot assign requested address");
	case ENETDOWN:		return ("Network is down");
	case ENETUNREACH:	return ("Network is unreachable");
	case ENETRESET:		return ("Network dropped connection on reset");
	case ECONNABORTED:	return ("Software caused connection abort");
	case ECONNRESET:	return ("Connection reset by peer");
	case ENOBUFS:		return ("No buffer space available");
	case EISCONN:		return ("Transport endpoint is already connected");
	case ENOTCONN:		return ("Transport endpoint is not connected");
	case ESHUTDOWN:		return ("Cannot send after transport endpoint shutdown");
	case ETOOMANYREFS:	return ("Too many references: cannot splice");
	case ETIMEDOUT:		return ("Connection timed out");
	case ECONNREFUSED:	return ("Connection refused");
	case EHOSTDOWN:		return ("Host is down");
	case EHOSTUNREACH:	return ("No route to host");
	case EALREADY:		return ("Operation already in progress");
	case EINPROGRESS:	return ("Operation now in progress");
	case ESTALE:		return ("Stale file handle");

	/* ----- POSIX realtime / message queue codes ----- */
	case ENOMSG:		return ("No message of desired type");
	case EIDRM:			return ("Identifier removed");
	case EPROTO:		return ("Protocol error");
	case EMULTIHOP:		return ("Multihop attempted");
	case EBADMSG:		return ("Bad message");
	case EOVERFLOW:		return ("Value too large for defined data type");
	case EILSEQ:		return ("Invalid or incomplete multibyte sequence");
	case ECANCELED:		return ("Operation canceled");

	/* ----- OS-specific codes ----- */

# if defined(HAJ_OS_LINUX)

	case ENOSTR:		return ("Device not a stream");
	case ENODATA:		return ("No data available");
	case ETIME:			return ("Timer expired");
	case ENOSR:			return ("Out of streams resources");
	case ENONET:		return ("Machine is not on the network");
	case EREMOTE:		return ("Object is remote");
	case ENOLINK:		return ("Link has been severed");
	case EADV:			return ("Advertise error");
	case ESRMNT:		return ("Srmount error");
	case ECOMM:			return ("Communication error on send");
	case EDOTDOT:		return ("RFS specific error");
	case ENOTUNIQ:		return ("Name not unique on network");
	case EBADFD:		return ("File descriptor in bad state");
	case EREMCHG:		return ("Remote address changed");
	case ELIBACC:		return ("Cannot access a needed shared library");
	case ELIBBAD:		return ("Accessing a corrupted shared library");
	case ELIBSCN:		return (".lib section in a.out corrupted");
	case ELIBMAX:		return ("Attempting to link in too many shared libraries");
	case ELIBEXEC:		return ("Cannot exec a shared library directly");
	case ERESTART:		return ("Interrupted system call should be restarted");
	case ESTRPIPE:		return ("Streams pipe error");
	case EUSERS:		return ("Too many users");
	case EDQUOT:		return ("Disk quota exceeded");
	case EOWNERDEAD:	return ("Owner died");
	case ENOTRECOVERABLE: return ("State not recoverable");

# elif defined(HAJ_OS_FREEBSD)

	case EDEADLK:		return ("Resource deadlock avoided");
	case EPROCLIM:		return ("Too many processes");
	case EUSERS:		return ("Too many users");
	case EDQUOT:		return ("Disc quota exceeded");
	case EBADRPC:		return ("RPC struct is bad");
	case ERPCMISMATCH:	return ("RPC version wrong");
	case EPROGUNAVAIL:	return ("RPC prog. not avail");
	case EPROGMISMATCH:	return ("Program version wrong");
	case EPROCUNAVAIL:	return ("Bad procedure for program");
	case EFTYPE:		return ("Inappropriate file type or format");
	case EAUTH:			return ("Authentication error");
	case ENEEDAUTH:		return ("Need authenticator");
	case ENOATTR:		return ("Attribute not found");
	case ENOTCAPABLE:	return ("Capabilities insufficient");

# elif defined(HAJ_OS_DARWIN)

	case EDEADLK:		return ("Resource deadlock avoided");
	case EPROCLIM:		return ("Too many processes");
	case EUSERS:		return ("Too many users");
	case EDQUOT:		return ("Disc quota exceeded");
	case EBADRPC:		return ("RPC struct is bad");
	case ERPCMISMATCH:	return ("RPC version wrong");
	case EPROGUNAVAIL:	return ("RPC prog. not avail");
	case EPROGMISMATCH:	return ("Program version wrong");
	case EPROCUNAVAIL:	return ("Bad procedure for program");
	case EFTYPE:		return ("Inappropriate file type or format");
	case EAUTH:			return ("Authentication error");
	case ENEEDAUTH:		return ("Need authenticator");
	case EPWROFF:		return ("Device power is off");
	case EDEVERR:		return ("Device error");
	case EBADEXEC:		return ("Bad executable (or shared library)");
	case EBADARCH:		return ("Bad CPU type in executable");
	case ESHLIBVERS:	return ("Shared library version mismatch");
	case EBADMACHO:		return ("Malformed Mach-o file");
	case ENOATTR:		return ("Attribute not found");
	case ENOPOLICY:		return ("No such policy registered");
	case ENOTRECOVERABLE: return ("State not recoverable");
	case EOWNERDEAD:	return ("Previous owner died");
	case EQFULL:		return ("Interface output queue is full");
	case ENODATA:		return ("No data available");
	case ENOSR:			return ("Out of streams resources");
	case ENOSTR:		return ("Device not a stream");
	case ETIME:			return ("Timer expired");
	case ENOLINK:		return ("Link has been severed");

# elif defined(HAJ_OS_WINDOWS)

	/*
	 * Windows CRT errno values. Most are shared with Unix, but
	 * a few are Windows-specific.
	 */
	case EDEADLOCK:		return ("Resource deadlock avoided");
	case STRUNCATE:		return ("String was truncated");

# endif

	/* ----- Unknown code: format "Unknown error N" into a TLS buffer. ----- */
	default:
		snprintf(__haj_strerror_buf, sizeof(__haj_strerror_buf),
				 "Unknown error %d", errnum);
		return (__haj_strerror_buf);
	}
}
