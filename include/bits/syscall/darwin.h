/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file darwin.h
 * @brief Darwin (macOS, iOS) syscall numbers.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/24 15:55:54 by Moutig
 *
 * Source: <sys/syscall.h> from the XNU kernel.
 * Darwin prefixes every BSD-class syscall with 0x2000000.
 * Mach-class syscalls use 0x1000000, but we don't need them.
 *
 * Do not include this header directly; include <bits/syscall.h>.
 */

#ifndef _BITS_SYSCALL_DARWIN_H
# define _BITS_SYSCALL_DARWIN_H

# define HAJ_DARWIN_BSD_CLASS	0x2000000
# define SYS_BSD(n)				(HAJ_DARWIN_BSD_CLASS | (n))

/* ---- Process control ---- */
# define SYS_exit				SYS_BSD(1)
# define SYS_fork				SYS_BSD(2)
# define SYS_wait4				SYS_BSD(7)
# define SYS_getpid				SYS_BSD(20)
# define SYS_getppid			SYS_BSD(39)
# define SYS_getuid				SYS_BSD(24)
# define SYS_geteuid			SYS_BSD(25)
# define SYS_getgid				SYS_BSD(47)
# define SYS_getegid			SYS_BSD(43)
# define SYS_getgroups			SYS_BSD(79)
# define SYS_setuid				SYS_BSD(23)
# define SYS_setgid				SYS_BSD(181)
# define SYS_kill				SYS_BSD(37)
# define SYS_setpgid			SYS_BSD(82)
# define SYS_getpgid			SYS_BSD(207)
# define SYS_setsid				SYS_BSD(147)
# define SYS_getsid				SYS_BSD(310)
# define SYS_execve				SYS_BSD(59)
# define SYS_posix_spawn		SYS_BSD(244)

/* ---- File I/O ---- */
# define SYS_read				SYS_BSD(3)
# define SYS_write				SYS_BSD(4)
# define SYS_open				SYS_BSD(5)
# define SYS_close				SYS_BSD(6)
# define SYS_lseek				SYS_BSD(199)
# define SYS_pread				SYS_BSD(153)
# define SYS_pwrite				SYS_BSD(154)
# define SYS_readv				SYS_BSD(120)
# define SYS_writev				SYS_BSD(121)
# define SYS_ioctl				SYS_BSD(54)
# define SYS_getdirentries		SYS_BSD(196)
# define SYS_fcntl				SYS_BSD(92)
# define SYS_flock				SYS_BSD(131)
# define SYS_fsync				SYS_BSD(95)
# define SYS_truncate			SYS_BSD(200)
# define SYS_ftruncate			SYS_BSD(201)
# define SYS_stat				SYS_BSD(188)
# define SYS_fstat				SYS_BSD(189)
# define SYS_lstat				SYS_BSD(190)
# define SYS_openat				SYS_BSD(463)
# define SYS_mkdirat			SYS_BSD(475)
# define SYS_renameat			SYS_BSD(465)
# define SYS_unlinkat			SYS_BSD(468)
# define SYS_linkat				SYS_BSD(466)
# define SYS_symlinkat			SYS_BSD(469)
# define SYS_readlinkat			SYS_BSD(472)
# define SYS_fchmodat			SYS_BSD(461)
# define SYS_fchownat			SYS_BSD(462)
# define SYS_faccessat			SYS_BSD(439)
# define SYS_newfstatat			SYS_BSD(470)
# define SYS_chmod				SYS_BSD(15)
# define SYS_fchmod				SYS_BSD(124)
# define SYS_chown				SYS_BSD(16)
# define SYS_fchown				SYS_BSD(123)
# define SYS_lchown				SYS_BSD(222)
# define SYS_umask				SYS_BSD(60)
# define SYS_dup				SYS_BSD(41)
# define SYS_dup2				SYS_BSD(90)
# define SYS_pipe				SYS_BSD(42)
# define SYS_mknod				SYS_BSD(14)
# define SYS_statfs				SYS_BSD(397)
# define SYS_fstatfs			SYS_BSD(398)
# define SYS_getcwd				SYS_BSD(326)
# define SYS_chdir				SYS_BSD(12)
# define SYS_fchdir				SYS_BSD(13)
# define SYS_rename				SYS_BSD(128)
# define SYS_mkdir				SYS_BSD(136)
# define SYS_rmdir				SYS_BSD(137)
# define SYS_link				SYS_BSD(9)
# define SYS_unlink				SYS_BSD(10)
# define SYS_symlink			SYS_BSD(57)
# define SYS_readlink			SYS_BSD(58)
# define SYS_getpriority		SYS_BSD(100)
# define SYS_setpriority		SYS_BSD(96)
# define SYS_sched_yield		SYS_BSD(331)
# define SYS_nanosleep			SYS_BSD(240)
# define SYS_getitimer			SYS_BSD(86)
# define SYS_setitimer			SYS_BSD(83)
# define SYS_select				SYS_BSD(93)
# define SYS_poll				SYS_BSD(230)
# define SYS_ppoll				SYS_BSD(309)
# define SYS_kqueue				SYS_BSD(362)
# define SYS_kevent				SYS_BSD(363)

/* ---- Memory ---- */
# define SYS_mmap				SYS_BSD(197)
# define SYS_munmap				SYS_BSD(73)
# define SYS_mprotect			SYS_BSD(74)
# define SYS_madvise			SYS_BSD(75)
# define SYS_mincore			SYS_BSD(78)
# define SYS_mlock				SYS_BSD(203)
# define SYS_munlock			SYS_BSD(204)
# define SYS_mlockall			SYS_BSD(324)
# define SYS_munlockall			SYS_BSD(325)
# define SYS_msync				SYS_BSD(65)

/* ---- Signals ---- */
# define SYS_sigaction			SYS_BSD(46)
# define SYS_sigprocmask		SYS_BSD(48)
# define SYS_sigaltstack		SYS_BSD(53)
# define SYS_sigsuspend			SYS_BSD(111)
# define SYS_sigpending			SYS_BSD(52)
# define SYS_sigwaitinfo		SYS_BSD(346)
# define SYS_sigtimedwait		SYS_BSD(345)
# define SYS_sigqueue			SYS_BSD(456)
# define SYS_tgkill				SYS_BSD(494)

/* ---- Threads ---- */
# define SYS_futex				SYS_BSD(454)
# define SYS_futex_wait			SYS_BSD(455)
# define SYS_futex_wake			SYS_BSD(456)
# define SYS_thread_selfid		SYS_BSD(372)
# define SYS_thread_switch		SYS_BSD(373)
# define SYS_sched_setaffinity	SYS_BSD(487)
# define SYS_sched_getaffinity	SYS_BSD(488)
# define SYS_sched_setscheduler	SYS_BSD(235)
# define SYS_sched_getscheduler	SYS_BSD(236)

/* ---- Time ---- */
# define SYS_gettimeofday		SYS_BSD(116)
# define SYS_settimeofday		SYS_BSD(122)
# define SYS_clock_gettime		SYS_BSD(228)
# define SYS_clock_settime		SYS_BSD(227)
# define SYS_clock_getres		SYS_BSD(229)
# define SYS_clock_nanosleep	SYS_BSD(244)
# define SYS_timer_create		SYS_BSD(235)
# define SYS_timer_settime		SYS_BSD(238)
# define SYS_timer_gettime		SYS_BSD(237)
# define SYS_timer_getoverrun	SYS_BSD(239)
# define SYS_timer_delete		SYS_BSD(236)

/* ---- Random ---- */
# define SYS_getentropy			SYS_BSD(500)

/* ---- Misc ---- */
# define SYS_uname				SYS_BSD(164)
# define SYS_sysctl				SYS_BSD(202)
# define SYS_getrlimit			SYS_BSD(194)
# define SYS_setrlimit			SYS_BSD(195)
# define SYS_getrusage			SYS_BSD(117)
# define SYS_sysinfo			SYS_BSD(398)

#endif /* _BITS_SYSCALL_DARWIN_H */
