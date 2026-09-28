/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file windows.h
 * @brief Windows pseudo-syscall numbers.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 15:56:49 by Moutig
 *
 * Windows has no stable syscall ABI. These numbers are used
 * only inside src/syscall/windows/__haj_syscall6.c to dispatch
 * to the mingw CRT.
 *
 * Do not include this header directly; include <bits/syscall.h>.
 */

#ifndef _BITS_SYSCALL_WINDOWS_H
# define _BITS_SYSCALL_WINDOWS_H

# define SYS_read				0x10001
# define SYS_write				0x10002
# define SYS_open				0x10003
# define SYS_close				0x10004
# define SYS_lseek				0x10005
# define SYS_stat				0x10006
# define SYS_fstat				0x10007
# define SYS_lstat				0x10008
# define SYS_mmap				0x10009
# define SYS_munmap				0x1000A
# define SYS_dup				0x1000B
# define SYS_dup2				0x1000C
# define SYS_getpid				0x1000D
# define SYS_exit				0x1000E
# define SYS_getcwd				0x1000F
# define SYS_chdir				0x10010
# define SYS_unlink				0x10011
# define SYS_mkdir				0x10012
# define SYS_rmdir				0x10013
# define SYS_rename				0x10014
# define SYS_access				0x10015
# define SYS_gettimeofday		0x10016
# define SYS_clock_gettime		0x10017
# define SYS_nanosleep			0x10018
# define SYS_getuid				0x10019
# define SYS_getgid				0x1001A
# define SYS_getppid			0x1001B
# define SYS_kill				0x1001C

#endif /* _BITS_SYSCALL_WINDOWS_H */
