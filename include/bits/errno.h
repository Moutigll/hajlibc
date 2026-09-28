/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file errno.h
 * @brief Error codes per OS.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 16:00:16 by Moutig
 *
 * This header defines the error codes (EPERM, ENOENT, ...) with
 * the values used by the target OS. The values are ABI-specific:
 * Linux, FreeBSD, and Darwin all use different numbering.
 *
 * We only declare the codes used by hajlib. If you need others,
 * add them here with the correct per-OS value.
 *
 * Do NOT include this header directly from user code. Use
 * <errno.h> instead.
 */

#ifndef _BITS_ERRNO_H
# define _BITS_ERRNO_H

# include <bits/os.h>

/* ----- Linux ----- */

# if defined(HAJ_OS_LINUX)

#  define EPERM				1	/* Operation not permitted */
#  define ENOENT			2	/* No such file or directory */
#  define ESRCH				3	/* No such process */
#  define EINTR				4	/* Interrupted system call */
#  define EIO				5	/* I/O error */
#  define ENXIO				6	/* No such device or address */
#  define E2BIG				7	/* Argument list too long */
#  define ENOEXEC			8	/* Exec format error */
#  define EBADF				9	/* Bad file descriptor */
#  define ECHILD			10	/* No child processes */
#  define EAGAIN			11	/* Try again */
#  define EWOULDBLOCK		EAGAIN	/* Operation would block */
#  define ENOMEM			12	/* Out of memory */
#  define EACCES			13	/* Permission denied */
#  define EFAULT			14	/* Bad address */
#  define ENOTBLK			15	/* Block device required */
#  define EBUSY				16	/* Device or resource busy */
#  define EEXIST			17	/* File exists */
#  define EXDEV				18	/* Cross-device link */
#  define ENODEV			19	/* No such device */
#  define ENOTDIR			20	/* Not a directory */
#  define EISDIR			21	/* Is a directory */
#  define EINVAL			22	/* Invalid argument */
#  define ENFILE			23	/* File table overflow */
#  define EMFILE			24	/* Too many open files */
#  define ENOTTY			25	/* Not a typewriter */
#  define ETXTBSY			26	/* Text file busy */
#  define EFBIG				27	/* File too large */
#  define ENOSPC			28	/* No space left on device */
#  define ESPIPE			29	/* Illegal seek */
#  define EROFS				30	/* Read-only file system */
#  define EMLINK			31	/* Too many links */
#  define EPIPE				32	/* Broken pipe */
#  define EDOM				33	/* Math argument out of domain */
#  define ERANGE			34	/* Math result not representable */
#  define EDEADLK			35	/* Resource deadlock avoided */
#  define ENAMETOOLONG		36	/* File name too long */
#  define ENOLCK			37	/* No locks available */
#  define ENOSYS			38	/* Function not implemented */
#  define ENOTEMPTY			39	/* Directory not empty */
#  define ELOOP				40	/* Too many symbolic links */
#  define ENOMSG			42	/* No message of desired type */
#  define EIDRM				43	/* Identifier removed */
#  define ENOSTR			60	/* Device not a stream */
#  define ENODATA			61	/* No data available */
#  define ETIME				62	/* Timer expired */
#  define ENOSR				63	/* Out of streams resources */
#  define ENONET			64	/* Machine is not on the network */
#  define EREMOTE			66	/* Object is remote */
#  define ENOLINK			67	/* Link has been severed */
#  define EADV				68	/* Advertise error */
#  define ESRMNT			69	/* Srmount error */
#  define ECOMM				70	/* Communication error on send */
#  define EPROTO			71	/* Protocol error */
#  define EMULTIHOP			72	/* Multihop attempted */
#  define EDOTDOT			73	/* RFS specific error */
#  define EBADMSG			74	/* Bad message */
#  define EOVERFLOW			75	/* Value too large for defined type */
#  define ENOTUNIQ			76	/* Name not unique on network */
#  define EBADFD			77	/* File descriptor in bad state */
#  define EREMCHG			78	/* Remote address changed */
#  define ELIBACC			79	/* Cannot access shared library */
#  define ELIBBAD			80	/* Accessing a corrupted shared library */
#  define ELIBSCN			81	/* .lib section in a.out corrupted */
#  define ELIBMAX			82	/* Attempting to link in too many shared libs */
#  define ELIBEXEC			83	/* Cannot exec a shared library directly */
#  define EILSEQ			84	/* Invalid or incomplete multibyte sequence */
#  define ERESTART			85	/* Interrupted system call should be restarted */
#  define ESTRPIPE			86	/* Streams pipe error */
#  define EUSERS			87	/* Too many users */
#  define ENOTSOCK			88	/* Socket operation on non-socket */
#  define EDESTADDRREQ		89	/* Destination address required */
#  define EMSGSIZE			90	/* Message too long */
#  define EPROTOTYPE		91	/* Protocol wrong type for socket */
#  define ENOPROTOOPT		92	/* Protocol not available */
#  define EPROTONOSUPPORT	93	/* Protocol not supported */
#  define ESOCKTNOSUPPORT	94	/* Socket type not supported */
#  define EOPNOTSUPP		95	/* Operation not supported on transport endpoint */
#  define ENOTSUP			EOPNOTSUPP
#  define EPFNOSUPPORT		96	/* Protocol family not supported */
#  define EAFNOSUPPORT		97	/* Address family not supported */
#  define EADDRINUSE		98	/* Address already in use */
#  define EADDRNOTAVAIL		99	/* Cannot assign requested address */
#  define ENETDOWN			100	/* Network is down */
#  define ENETUNREACH		101	/* Network is unreachable */
#  define ENETRESET			102	/* Network dropped connection on reset */
#  define ECONNABORTED		103	/* Software caused connection abort */
#  define ECONNRESET		104	/* Connection reset by peer */
#  define ENOBUFS			105	/* No buffer space available */
#  define EISCONN			106	/* Transport endpoint is already connected */
#  define ENOTCONN			107	/* Transport endpoint is not connected */
#  define ESHUTDOWN			108	/* Cannot send after transport endpoint shutdown */
#  define ETOOMANYREFS		109	/* Too many references: cannot splice */
#  define ETIMEDOUT			110	/* Connection timed out */
#  define ECONNREFUSED		111	/* Connection refused */
#  define EHOSTDOWN			112	/* Host is down */
#  define EHOSTUNREACH		113	/* No route to host */
#  define EALREADY			114	/* Operation already in progress */
#  define EINPROGRESS		115	/* Operation now in progress */
#  define ESTALE			116	/* Stale file handle */
#  define EDQUOT			122	/* Disk quota exceeded */
#  define ECANCELED			125	/* Operation canceled */
#  define EOWNERDEAD		130	/* Owner died */
#  define ENOTRECOVERABLE	131	/* State not recoverable */

/* ----- FreeBSD ----- */

# elif defined(HAJ_OS_FREEBSD)

#  define EPERM			1
#  define ENOENT		2
#  define ESRCH			3
#  define EINTR			4
#  define EIO			5
#  define ENXIO			6
#  define E2BIG			7
#  define ENOEXEC		8
#  define EBADF			9
#  define ECHILD		10
#  define EDEADLK		11
#  define ENOMEM		12
#  define EACCES		13
#  define EFAULT		14
#  define ENOTBLK		15
#  define EBUSY			16
#  define EEXIST		17
#  define EXDEV			18
#  define ENODEV		19
#  define ENOTDIR		20
#  define EISDIR		21
#  define EINVAL		22
#  define ENFILE		23
#  define EMFILE		24
#  define ENOTTY		25
#  define ETXTBSY		26
#  define EFBIG			27
#  define ENOSPC		28
#  define ESPIPE		29
#  define EROFS			30
#  define EMLINK		31
#  define EPIPE			32
#  define EDOM			33
#  define ERANGE		34
#  define EAGAIN		35
#  define EWOULDBLOCK	EAGAIN
#  define EINPROGRESS	36
#  define EALREADY		37
#  define ENOTSOCK		38
#  define EDESTADDRREQ	39
#  define EMSGSIZE		40
#  define EPROTOTYPE	41
#  define ENOPROTOOPT	42
#  define EPROTONOSUPPORT 43
#  define ESOCKTNOSUPPORT 44
#  define EOPNOTSUPP	45
#  define ENOTSUP		EOPNOTSUPP
#  define EPFNOSUPPORT	46
#  define EAFNOSUPPORT	47
#  define EADDRINUSE	48
#  define EADDRNOTAVAIL	49
#  define ENETDOWN		50
#  define ENETUNREACH	51
#  define ENETRESET		52
#  define ECONNABORTED	53
#  define ECONNRESET	54
#  define ENOBUFS		55
#  define EISCONN		56
#  define ENOTCONN		57
#  define ESHUTDOWN		58
#  define ETOOMANYREFS	59
#  define ETIMEDOUT		60
#  define ECONNREFUSED	61
#  define ELOOP			62
#  define ENAMETOOLONG	63
#  define EHOSTDOWN		64
#  define EHOSTUNREACH	65
#  define ENOTEMPTY		66
#  define EPROCLIM		67
#  define EUSERS		68
#  define EDQUOT		69
#  define ESTALE		70
#  define EREMOTE		71
#  define EBADRPC		72
#  define ERPCMISMATCH	73
#  define EPROGUNAVAIL	74
#  define EPROGMISMATCH	75
#  define EPROCUNAVAIL	76
#  define ENOLCK		77
#  define ENOSYS		78
#  define EFTYPE		79
#  define EAUTH			80
#  define ENEEDAUTH		81
#  define EIDRM			82
#  define ENOMSG		83
#  define EOVERFLOW		84
#  define ECANCELED		85
#  define EILSEQ		86
#  define ENOATTR		87
#  define EBADMSG		89
#  define EMULTIHOP		90
#  define ENOLINK		91
#  define EPROTO		92
#  define ENOTCAPABLE	93

/* ----- Darwin (macOS, iOS) ----- */

# elif defined(HAJ_OS_DARWIN)

#  define EPERM			1
#  define ENOENT		2
#  define ESRCH			3
#  define EINTR			4
#  define EIO			5
#  define ENXIO			6
#  define E2BIG			7
#  define ENOEXEC		8
#  define EBADF			9
#  define ECHILD		10
#  define EDEADLK		11
#  define ENOMEM		12
#  define EACCES		13
#  define EFAULT		14
#  define ENOTBLK		15
#  define EBUSY			16
#  define EEXIST		17
#  define EXDEV			18
#  define ENODEV		19
#  define ENOTDIR		20
#  define EISDIR		21
#  define EINVAL		22
#  define ENFILE		23
#  define EMFILE		24
#  define ENOTTY		25
#  define ETXTBSY		26
#  define EFBIG			27
#  define ENOSPC		28
#  define ESPIPE		29
#  define EROFS			30
#  define EMLINK		31
#  define EPIPE			32
#  define EDOM			33
#  define ERANGE		34
#  define EAGAIN		35
#  define EWOULDBLOCK	EAGAIN
#  define EINPROGRESS	36
#  define EALREADY		37
#  define ENOTSOCK		38
#  define EDESTADDRREQ	39
#  define EMSGSIZE		40
#  define EPROTOTYPE	41
#  define ENOPROTOOPT	42
#  define EPROTONOSUPPORT 43
#  define ESOCKTNOSUPPORT 44
#  define ENOTSUP		45
#  define EPFNOSUPPORT	46
#  define EAFNOSUPPORT	47
#  define EADDRINUSE	48
#  define EADDRNOTAVAIL	49
#  define ENETDOWN		50
#  define ENETUNREACH	51
#  define ENETRESET		52
#  define ECONNABORTED	53
#  define ECONNRESET	54
#  define ENOBUFS		55
#  define EISCONN		56
#  define ENOTCONN		57
#  define ESHUTDOWN		58
#  define ETOOMANYREFS	59
#  define ETIMEDOUT		60
#  define ECONNREFUSED	61
#  define ELOOP			62
#  define ENAMETOOLONG	63
#  define EHOSTDOWN		64
#  define EHOSTUNREACH	65
#  define ENOTEMPTY		66
#  define EPROCLIM		67
#  define EUSERS		68
#  define EDQUOT		69
#  define ESTALE		70
#  define EREMOTE		71
#  define EBADRPC		72
#  define ERPCMISMATCH	73
#  define EPROGUNAVAIL	74
#  define EPROGMISMATCH	75
#  define EPROCUNAVAIL	76
#  define ENOLCK		77
#  define ENOSYS		78
#  define EFTYPE		79
#  define EAUTH			80
#  define ENEEDAUTH		81
#  define EPWROFF		82
#  define EDEVERR		83
#  define EOVERFLOW		84
#  define EBADEXEC		85
#  define EBADARCH		86
#  define ESHLIBVERS		87
#  define EBADMACHO		88
#  define ECANCELED		89
#  define EIDRM			90
#  define ENOMSG		91
#  define EILSEQ		92
#  define ENOATTR		93
#  define EBADMSG		94
#  define EMULTIHOP		95
#  define ENODATA		96
#  define ENOLINK		97
#  define ENOSR			98
#  define ENOSTR		99
#  define EPROTO		100
#  define ETIME			101
#  define EOPNOTSUPP	102
#  define ENOPOLICY		103
#  define ENOTRECOVERABLE 104
#  define EOWNERDEAD	105
#  define EQFULL		106

/* ----- Windows (mingw CRT) ----- */

# elif defined(HAJ_OS_WINDOWS)

/*
 * Windows CRT errno values. These are different from the Unix
 * values but the mingw CRT uses them.
 */
#  define EPERM			1
#  define ENOENT		2
#  define ESRCH			3
#  define EINTR			4
#  define EIO			5
#  define ENXIO			6
#  define E2BIG			7
#  define ENOEXEC		8
#  define EBADF			9
#  define ECHILD		10
#  define EAGAIN		11
#  define EWOULDBLOCK	EAGAIN
#  define ENOMEM		12
#  define EACCES		13
#  define EFAULT		14
#  define EBUSY			16
#  define EEXIST		17
#  define EXDEV			18
#  define ENODEV		19
#  define ENOTDIR		20
#  define EISDIR		21
#  define EINVAL		22
#  define ENFILE		23
#  define EMFILE		24
#  define ENOTTY		25
#  define EFBIG			27
#  define ENOSPC		28
#  define ESPIPE		29
#  define EROFS			30
#  define EMLINK		31
#  define EPIPE			32
#  define EDOM			33
#  define ERANGE		34
#  define EDEADLK		36
#  define EDEADLOCK		EDEADLK
#  define ENAMETOOLONG	38
#  define ENOLCK		39
#  define ENOSYS		40
#  define ENOTEMPTY		41
#  define EILSEQ		42
#  define STRUNCATE		80

/* ----- Unknown ----- */

# else

/*
 * Fallback: define a minimal set of errno values. If you are
 * porting hajlib to a new OS, add a proper block above.
 */

#  define EPERM			1
#  define ENOENT		2
#  define EINTR			4
#  define EIO			5
#  define EBADF			9
#  define EAGAIN		11
#  define ENOMEM		12
#  define EACCES		13
#  define EFAULT		14
#  define EEXIST		17
#  define EINVAL		22
#  define ENFILE		23
#  define EMFILE		24
#  define ENOSPC		28
#  define EPIPE			32
#  define ERANGE		34

# endif

/* ----- Convenience ----- */

/*
 * Maximum errno value. Used by strerror to size its table.
 * The exact value depends on the OS; this is a safe upper bound.
 */
# define __HAJ_ERRNO_MAX	256

#endif /* _BITS_ERRNO_H */
