/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Moutig <ele-lean@moutig.sh>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file freebsd.h
 * @brief FreeBSD syscall numbers.
 * @Created: 2026/09/24 15:06:42 by Moutig
 * @Updated: 2026/09/28 10:39:20 by Moutig
 *
 * Source: <sys/syscall.h> from the FreeBSD kernel.
 * FreeBSD uses the same syscall numbers on all 64-bit
 * architectures (amd64, arm64). This is a deliberate design
 * choice of the FreeBSD kernel.
 *
 * Do not include this header directly; include <bits/syscall.h>.
 */

#ifndef _BITS_SYSCALL_FREEBSD_H
# define _BITS_SYSCALL_FREEBSD_H

/* ---- Process control ---- */
# define SYS_exit				1
# define SYS_fork				2
# define SYS_wait4				7
# define SYS_getpid				20
# define SYS_getppid			39
# define SYS_getuid				24
# define SYS_geteuid			25
# define SYS_getgid				47
# define SYS_getegid			43
# define SYS_getgroups			79
# define SYS_setuid				23
# define SYS_setgid				181
# define SYS_kill				37
# define SYS_setpgid			82
# define SYS_getpgid			207
# define SYS_setsid				147
# define SYS_getsid				310
# define SYS_execve				59
# define SYS_clone				359

/* ---- File I/O ---- */
# define SYS_read				3
# define SYS_write				4
# define SYS_open				5
# define SYS_close				6
# define SYS_lseek				478
# define SYS_pread				475
# define SYS_pwrite				476
# define SYS_readv				120
# define SYS_writev				121
# define SYS_ioctl				54
# define SYS_getdents			272
# define SYS_fcntl				92
# define SYS_flock				131
# define SYS_fsync				95
# define SYS_fdatasync			550
# define SYS_truncate			480
# define SYS_ftruncate			481
# define SYS_stat				188
# define SYS_fstat				189
# define SYS_lstat				190
# define SYS_openat				499
# define SYS_mkdirat			503
# define SYS_renameat			501
# define SYS_unlinkat			504
# define SYS_linkat				502
# define SYS_symlinkat			505
# define SYS_readlinkat			506
# define SYS_fchmodat			490
# define SYS_fchownat			491
# define SYS_faccessat			489
# define SYS_newfstatat			552
# define SYS_chmod				15
# define SYS_fchmod				124
# define SYS_chown				16
# define SYS_fchown				123
# define SYS_lchown				222
# define SYS_umask				60
# define SYS_dup				41
# define SYS_dup2				90
# define SYS_pipe				42
# define SYS_pipe2				542
# define SYS_mknod				14
# define SYS_statfs				397
# define SYS_fstatfs			398
# define SYS_getcwd				326
# define SYS_chdir				12
# define SYS_fchdir				13
# define SYS_rename				128
# define SYS_mkdir				136
# define SYS_rmdir				137
# define SYS_creat				182
# define SYS_link				9
# define SYS_unlink				10
# define SYS_symlink			57
# define SYS_readlink			58
# define SYS_getpriority		100
# define SYS_setpriority		96
# define SYS_sched_yield		331
# define SYS_nanosleep			240
# define SYS_getitimer			86
# define SYS_setitimer			83
# define SYS_select				93
# define SYS_poll				209
# define SYS_pselect			522
# define SYS_ppoll				545
# define SYS_kqueue				362
# define SYS_kevent				363

/* ---- Memory ---- */
# define SYS_mmap				477
# define SYS_munmap				73
# define SYS_mprotect			74
# define SYS_madvise			75
# define SYS_mincore			78
# define SYS_mlock				203
# define SYS_munlock			204
# define SYS_mlockall			324
# define SYS_munlockall			325
# define SYS_msync				65
# define SYS_break				17
# define SYS_shm_open			482
# define SYS_shm_unlink			483

/* ---- Signals ---- */
# define SYS_sigaction			46
# define SYS_sigprocmask		48
# define SYS_sigreturn			417
# define SYS_sigaltstack		53
# define SYS_sigsuspend			111
# define SYS_sigpending			52
# define SYS_sigwaitinfo		346
# define SYS_sigtimedwait		345
# define SYS_sigqueue			456
# define SYS_tgkill				494

/* ---- Threads ---- */
# define SYS_futex				449
# define SYS_futex_wait			450
# define SYS_futex_wake			451
# define SYS_sched_setaffinity	487
# define SYS_sched_getaffinity	488
# define SYS_sched_setparam		233
# define SYS_sched_getparam		234
# define SYS_sched_setscheduler	235
# define SYS_sched_getscheduler	236

/* ---- Time ---- */
# define SYS_time				13
# define SYS_gettimeofday		116
# define SYS_settimeofday		122
# define SYS_clock_gettime		232
# define SYS_clock_settime		231
# define SYS_clock_getres		234
# define SYS_clock_nanosleep	244
# define SYS_timer_create		234
# define SYS_timer_settime		238
# define SYS_timer_gettime		237
# define SYS_timer_getoverrun	239
# define SYS_timer_delete		236

/* ---- Random ---- */
# define SYS_getrandom			563

/* ---- Misc ---- */
# define SYS_uname				164
# define SYS_sysarch			165
# define SYS_sysctl				202
# define SYS_getrlimit			194
# define SYS_setrlimit			195
# define SYS_getrusage			117
# define SYS_sysinfo			398

#endif /* _BITS_SYSCALL_FREEBSD_H */
