/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file __haj_syscall6.c
 * @brief Windows fallback for __haj_syscall6.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 18:32:34 by Moutig
 *
 * Windows has no stable syscall ABI. The numbers of the NT
 * syscalls change between Windows builds, and Microsoft does not
 * document them as a stable interface. Applications are expected
 * to use the Win32 API (ReadFile, WriteFile, CreateFile, etc.)
 * or the C runtime (CRT).
 *
 * This implementation dispatches the pseudo-syscall numbers
 * defined in <bits/syscall/windows.h> to the mingw CRT. It is
 * NOT a real syscall; it is a compatibility shim that lets the
 * rest of hajlib use a uniform interface across OSes.
 *
 * The pseudo-numbers are in the range 0x10000 to 0x1FFFF, so they
 * do not collide with any real syscall number on Linux, FreeBSD,
 * or Darwin.
 *
 * Return convention (same as Unix):
 *   >= 0 : success, value is the result.
 *   < 0  : error, value is -errno.
 */

#include <bits/syscall.h>

#if defined(HAJ_OS_WINDOWS)

#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <process.h>
#include <direct.h>
#include <errno.h>

/* ----- Flag translation -----
 *
 * hajlib uses POSIX O_* flags in its public headers. The mingw
 * CRT uses its own _O_* flags. We translate them here.
 */

static int	translate_open_flags(int haj_flags)
{
	int	win_flags = 0;

	switch (haj_flags & 0x3) {
	case O_RDONLY:
		win_flags |= _O_RDONLY;
		break;
	case O_WRONLY:
		win_flags |= _O_WRONLY;
		break;
	case O_RDWR:
		win_flags |= _O_RDWR;
		break;
	}

	if (haj_flags & O_CREAT)
		win_flags |= _O_CREAT;
	if (haj_flags & O_EXCL)
		win_flags |= _O_EXCL;
	if (haj_flags & O_TRUNC)
		win_flags |= _O_TRUNC;
	if (haj_flags & O_APPEND)
		win_flags |= _O_APPEND;

	/*
	 * Windows distinguishes text and binary mode. On Unix there
	 * is no such distinction. Default to binary to match Unix.
	 */
	win_flags |= _O_BINARY;

	return (win_flags);
}

/* ----- Error normalization  -----
 *
 * On Windows, the CRT sets errno to a POSIX-like value when a
 * function fails. We return -errno to match Unix.
 */

static long	normalize_result(long ret)
{
	if (ret < 0)
		return (-errno);
	return (ret);
}

/* ----- Dispatch ----- */

long	__haj_syscall6(long nr, long a1, long a2, long a3,
                       long a4, long a5, long a6)
{
	long	ret;

	(void)a4;
	(void)a5;
	(void)a6;

	switch (nr) {
	/* ---- File I/O ---- */
	case SYS_read:
		ret = (long)_read((int)a1, (void *)a2, (unsigned)a3);
		return (normalize_result(ret));

	case SYS_write:
		ret = (long)_write((int)a1, (const void *)a2, (unsigned)a3);
		return (normalize_result(ret));

	case SYS_open:
		ret = (long)_open((const char *)a1,
		                  translate_open_flags((int)a2),
		                  (int)a3);
		return (normalize_result(ret));

	case SYS_close:
		ret = (long)_close((int)a1);
		return (normalize_result(ret));

	case SYS_lseek:
		ret = (long)_lseek((int)a1, (long)a2, (int)a3);
		return (normalize_result(ret));

	case SYS_fstat:
		ret = (long)_fstat((int)a1, (struct _stat *)a2);
		return (normalize_result(ret));

	case SYS_stat:
		ret = (long)_stat((const char *)a1, (struct _stat *)a2);
		return (normalize_result(ret));

	/* ---- Process control ---- */
	case SYS_getpid:
		ret = (long)_getpid();
		return (ret);

	case SYS_getppid:
		/*
		 * Windows does not have getppid. Return the same
		 * pid as getpid, as a fallback.
		 */
		ret = (long)_getpid();
		return (ret);

	case SYS_exit:
		_exit((int)a1);
		/* _exit does not return. */
		return (0);

	case SYS_kill:
		return (-ENOSYS);

	/* ---- File descriptors ---- */
	case SYS_dup:
		ret = (long)_dup((int)a1);
		return (normalize_result(ret));

	case SYS_dup2:
		ret = (long)_dup2((int)a1, (int)a2);
		return (normalize_result(ret));

	/* ---- File system ---- */
	case SYS_getcwd:
		ret = (long)_getcwd((char *)a1, (int)a2);
		return (normalize_result(ret));

	case SYS_chdir:
		ret = (long)_chdir((const char *)a1);
		return (normalize_result(ret));

	case SYS_unlink:
		ret = (long)_unlink((const char *)a1);
		return (normalize_result(ret));

	case SYS_mkdir:
		ret = (long)_mkdir((const char *)a1);
		return (normalize_result(ret));

	case SYS_rmdir:
		ret = (long)_rmdir((const char *)a1);
		return (normalize_result(ret));

	case SYS_rename:
		ret = (long)_rename((const char *)a1, (const char *)a2);
		return (normalize_result(ret));

	case SYS_access:
		ret = (long)_access((const char *)a1, (int)a2);
		return (normalize_result(ret));

	/* ---- Memory ---- */
	case SYS_mmap:
		/*
		 * The mmap implementation for Windows uses
		 * VirtualAlloc directly. It does not go through
		 * this dispatcher.
		 */
		return (-ENOSYS);

	case SYS_munmap:
		return (-ENOSYS);

	/* ---- Time ---- */
	case SYS_gettimeofday:
		return (-ENOSYS);

	case SYS_clock_gettime:
		return (-ENOSYS);

	case SYS_nanosleep:
		{
			long	ms = a1 / 1000000L;

			if (ms > 0)
				Sleep((DWORD)ms);
			return (0);
		}

	/* ---- User / group ---- */
	case SYS_getuid:
	case SYS_getgid:
		/*
		 * Windows does not have POSIX user/group IDs.
		 */
		return (0);

	/* ---- Random ---- */
	case SYS_getrandom:
		return (__haj_windows_getrandom((void *)a1,
		                                (unsigned long)a2));

	/* ---- Unknown ---- */
	default:
		return (-ENOSYS);
	}
}

#endif /* defined(HAJ_OS_WINDOWS) */
