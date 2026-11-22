# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

#### Number conversion (`src/stdlib/strto/`)

- `strtol`, `strtoll` and `strtoimax`, generated from one template
  (`strtoInt.templ.inl`) parameterised by type and limits: leading
  white space, optional sign, bases 2 to 36, base 0 picking 8, 10 or
  16 from the prefix, and `0x` consumed only if a hex digit follows
  (`"0xz"` parses as 0 with `*endptr` on the `x`)
- Range errors return the type's `MIN`/`MAX` with `errno = ERANGE` and
  still consume every digit; an invalid base returns 0 with
  `errno = EINVAL`; when no digit is read, `*endptr` is `nptr`
- `strtoul`, `strtoull` and `strtoumax`, generated from a matching
  unsigned template (`strtoUint.templ.inl`) with the same parsing
  rules; a `-` sign negates the result in the unsigned type (`"-1"`
  gives `ULONG_MAX`), and out-of-range input returns the type's `MAX`
  with `errno = ERANGE`, even with a `-` sign
- `atoi`, `atol` and `atoll`, thin base-10 wrappers over `strtol` /
  `strtoll` (no error reporting, as the standard allows)
- Internal `__haj_digit` helper (`digit.h`), shared by the strto
  family

#### Greatest-width integers (`src/stdlib/`)

- `imaxabs` and `imaxdiv`

#### Standard I/O (`src/stdio/`)

- `fopen`, `fdopen`, `freopen`, `fclose` and `fileno`, on top of an
  internal `struct FILE` with pluggable read / write / seek / close
  backends (file-descriptor backend for now)
- Mode strings `r`, `w`, `a` with the `+`, `b`, `x` (`O_EXCL`) and
  `e` (`O_CLOEXEC`) modifiers; `a+` keeps `O_APPEND`
- Buffer sized from `st_blksize` (`BUFSIZ` if `fstat` fails), allocated
  on open; if `malloc` fails the stream is unbuffered instead of the
  open failing. Terminals are line-buffered, other files fully buffered
- Static `stdin` (line-buffered), `stdout` (line-buffered) and
  `stderr` (unbuffered), usable before `malloc` runs; `fclose` on them
  flushes but keeps the object, so `freopen` can reuse it
- Thread safety: per-`FILE` recursive futex lock, skipped while the
  program has a single thread, and a global open-file list for
  `fflush(NULL)` and `exit`
- `freopen` with a NULL path (mode change) fails with `EINVAL`
- `fflush`: writes pending output through the backend; on a seekable
  input stream, rewinds the descriptor over the unread bytes so a
  following `read(2)` sees them. `fflush(NULL)` flushes `stdin`,
  `stdout` and `stderr`, then walks the open-file list with a trylock
  on each stream, and returns `EOF` if any stream failed
- `setvbuf` (`_IOFBF`, `_IOLBF`, `_IONBF`, caller buffer or a
  stdio-allocated one, `BUFSIZ` when `size` is 0) and `setbuf` on top
  of it. The new buffer is allocated before the old one is freed, so
  a `malloc` failure leaves the stream untouched; a caller buffer of
  size 0 and a second `setvbuf` on the same stream fail with `EINVAL`
  (`freopen` re-allows it)
- `fread` and `fwrite`, under the per-`FILE` lock: buffered bytes are
  consumed first, requests at least as large as the buffer bypass it,
  and unbuffered streams write through. Line-buffered streams flush
  when the chunk just copied holds a newline. `fread` loops on short
  reads and sets the EOF indicator on a zero read; a `size * nmemb`
  overflow fails with `EOVERFLOW`
- The fd read backend does a single `read(2)`, so a buffer refill on
  a terminal or a pipe returns as soon as data is available instead
  of waiting for a full buffer
- `feof`, `ferror` and `clearerr` on the stream's end-of-file and
  error indicators
- `fseek`, `fseeko`, `ftell`, `ftello`, `rewind`, `fgetpos` and
  `fsetpos`: seeking flushes pending writes, drops the read window and
  clears the end-of-file indicator on success (`rewind` also clears
  the error indicator); `ftell` reports the logical position (kernel
  offset minus unread bytes) and fails with `EOVERFLOW` past
  `LONG_MAX`. A rejected seek (`ESPIPE`, `EINVAL`) does not set the
  error indicator. `fpos_t` holds a plain `off_t` (byte-oriented
  streams only)
- `clearerr` takes the stream lock
- `flockfile`, `ftrylockfile` and `funlockfile` over the per-`FILE`
  recursive lock. They always take the lock, even before the first
  `pthread_create`, so a stream locked by the user stays locked for
  threads created while it is held
- Internal stream locks are inline wrappers that skip the call while
  the process is single-threaded; the trylock used by `fflush(NULL)`
  no longer sets `errno`
- `exit` flushes every stream (step 4, after the `atexit` handlers
  and the destructors), with a trylock so a stream held by another
  thread is skipped instead of deadlocking; streams are synced, not
  closed. `_exit` still does not flush
- `fork` keeps stdio usable in the child: the open-file list lock is
  held across the fork, and in the child the locks of the forking
  thread are handed to the child's tid while those of the other
  (vanished) threads are cleared. The stdio hooks run after the
  user's `pthread_atfork` prepare handlers and before the child ones

#### File status and terminals

- `fstat`, thin wrapper over the `fstat` syscall
- `isatty`: `TCGETS` on the descriptor, 1 on success, 0 with
  `errno = ENOTTY` otherwise
- `ioctl`, variadic, third argument passed as `unsigned long`

#### Headers

- `<sys/stat.h>`: `fstat` declaration; `<bits/stat.h>`: `struct stat`
  matching the kernel layout of x86_64 and aarch64 (asm-generic),
  `st_atim` / `st_mtim` / `st_ctim` with the `st_atime` / `st_mtime` /
  `st_ctime` macros, the `S_IF*` type bits, the `S_IS*` tests and the
  permission bits
- `<termios.h>` and `<bits/termios.h>`: `struct termios` (kernel ABI)
  and the `TCGETS` / `TCSETS` / `TCSETSW` / `TCSETSF` requests
- `<sys/ioctl.h>`: `ioctl` declaration
- `<unistd.h>`: `isatty` declaration
- `<inttypes.h>`: `imaxdiv_t`, declarations of `imaxabs`, `imaxdiv`
  and `strtoimax`, and every printf / scanf format macro: `PRI*` and
  `SCN*` for `d i o u x` (plus `PRIX*`), for the exact-width (`8`,
  `16`, `32`, `64`), `LEAST`, `FAST`, `MAX` and `PTR` types. Their
  length modifiers follow the types picked by `<bits/types.h>`
  (`int64_t` is `long` on LP64, `long long` otherwise)
- `<stdlib.h>`: `strtol` and `strtoll` declarations
- `<stdio.h>`: `fpos_t`, `EOF`, the constants of `<bits/stdio.h>`,
  and the `fopen` / `fdopen` / `freopen` / `fclose` / `fileno`
  declarations; `snprintf` and `fprintf` stay declared for their
  existing callers but are not implemented yet
- `<bits/stdio.h>`: internal header for the constants shared by
  `<stdio.h>` and `<unistd.h>`: `SEEK_SET` / `SEEK_CUR` / `SEEK_END`,
  `BUFSIZ`, `FILENAME_MAX`, `FOPEN_MAX`, `L_ctermid`, `L_tmpnam`,
  `TMP_MAX`, and the `_IOFBF` / `_IOLBF` / `_IONBF` buffering modes

### Changed

- `<bits/types.h>`: `nlink_t` and `blksize_t` are 32 bits on aarch64,
  matching the asm-generic `struct stat`; they stay 64 bits
  on x86_64
- License headers also cover `.inl` templates: the pre-commit hook
  and `scripts/header-update.sh` handle `.inl`, `make headers-add`
  handles `.hpp`, `.cpp` and `.inl`, and the VS Code on-save update
  handles `.hpp`, `.cpp`, `.inl` and `.S`
- `<stdint.h>`: the `PRI*` format macros moved to `<inttypes.h>`,
  where the C standard and POSIX define them

### Fixed

- `pipe` passed no `flags` argument to `pipe2` (garbage in the second
  register, random `EINVAL`)
- `PRIdPTR`, `PRIiPTR`, `PRIuPTR`, `PRIxPTR` and `PRIXPTR` used the
  64-bit length modifier on 32-bit platforms, where `intptr_t` is an
  `int`

### Removed

- Nothing yet

### Deprecated

- Nothing yet

## [0.5.0] - 2026-11-18

This release brings hmalloc, hajlibc's own memory allocator: a
thread-caching, mmap-only allocator modelled on mimalloc, usable
from `libhajc.a` or as an `LD_PRELOAD` replacement for the system
`malloc`. It comes with a Valgrind-style leak report, statistics,
a sampling profiler, a 42-subject build profile
(`HMALLOC_FT42`), and the debug-info module (ELF, DWARF line
tables, `dl_iterate_phdr`) used to symbolize leak traces. The
release also adds `qsort`, `realpath`, `<stdatomic.h>`, real
per-thread TLS areas, and a reworked build system.

### Added

#### Memory allocator (`src/stdlib/malloc/`)

- `<malloc.h>`: `malloc`, `free`, `realloc`, `calloc` and the
  `h*` extension API (`hmallocAligned`, `husableSize`,
  `hgoodSize`, ...), `malloc_usable_size`, `malloc_size`,
  `malloc_good_size`, `cfree`, `vfree`, `_aligned_malloc`
- `<stdlib.h>`: `posix_memalign`, `aligned_alloc`, `memalign`,
  `valloc`, `pvalloc`, `reallocarray`, `reallocf`, `reallocarr`,
  C23 `free_sized` / `free_aligned_sized`; glibc aliases
  `__posix_memalign`, `__libc_memalign`, `__libc_valloc`
- C++ `operator new` / `operator delete` replacements (sized and
  aligned variants), also defined in C by their mangled names
  (`newdelete.c`) so the C-only library replaces them too
- Arenas: aligned, 64 KiB-sliced regions held in a fixed
  `subproc->arenas[]` table read without a lock; slices are
  tracked with concurrent two-level bitmaps (`hmBitmap.h`)
- Pages: tiny / small / large size classes (64 KiB / 512 KiB /
  4 MiB pages) with lazily extended free lists; larger blocks get
  their own mapping
- Thread-local heaps (theaps): lock-free fast path on the owner
  thread, deferred cross-thread frees, abandoned-page reclaim,
  retired pages kept at the head of their bin queue
- Address → page map (`hmPageMap.h`) used by checked `free`
- Deferred purge of unused slices (`madvise`), THP control
  (`prctl(PR_SET_THP_DISABLE)`, `MADV_NOHUGEPAGE`)
- Fork safety: child handler registered with `pthread_atfork`,
  theaps left by the fork are abandoned in the child
- Heap API: `hmSubprocMain`, `hmHeapMain`, `hmHeapOf`,
  `hmHeapContains`, `hmAnyHeapContains`, `hmHeapVisitBlocks`,
  `hmHeapVisitAbandonedBlocks`, `hmTheapGetDefault`
- Runtime options through `HMALLOC_*` environment variables
  (`SHOW_ERRORS`, `SHOW_STATS`, `VERBOSE`, `PURGE_DELAY`,
  `PAGE_FULL_RETAIN`, `GUARDED_SAMPLE_RATE`, `PERTURB`, `CHECK`,
  `CHECK_LEAKS`, `PAGE_RECLAIM_ON_FREE`, `PAGE_MAX_RECLAIM`,
  `PAGE_CROSS_THREAD_MAX_RECLAIM`, `GENERIC_COLLECT`,
  `ARENA_PURGE_MULT`, `ARENA_PURGE_MIN_SIZE`,
  `ARENA_PURGE_IMMEDIATE_SIZE`, `ALLOW_THP`) and `hmOptionGetSize`
- `<hmConfig.h>`: build-time knobs (`HMALLOC_DEBUG`, `CANARY`,
  `CHECK`, `LEAK_TRACE`, `ENCODE_FREELIST`, `LOG_*`, `PROFILE`,
  `STATS`, auto-detected ASAN / TSAN), all `-D` overridable with
  production-safe defaults
- Checked builds (`HMALLOC_CHECK`): low, misaligned, foreign and
  interior pointers are rejected and double frees reported by
  `free` and `realloc` (`realloc` then returns `NULL` / `EINVAL`
  and leaves the block untouched); the padding slack is painted
  and verified on free to catch small overflows
- Statistics: `hmStatsGet`, `hmStatsGetJson`, `hmStatsPrint`,
  per-subproc / heap / theap stats, `total` and `peak` counters;
  compiled to inline stubs when `HMALLOC_STATS=0`
- Sampling profiler (`hmProfilerStart` / `hmProfilerStop`),
  opt-in with `HMALLOC_PROFILE`, stubbed otherwise
- Introspection: `show_alloc_mem` (all zones in address order,
  TINY / SMALL / LARGE), `show_alloc_mem_ex`, `hmProcessInfo`
- Callbacks: `hmRegisterOutput`, `hmRegisterError`,
  `hmRegisterDeferredFree`
- Leak report (`hmReportLeaks`, automatic at exit with
  `HMALLOC_CHECK_LEAKS`): Valgrind-style sections and
  `LEAK SUMMARY` on stderr without stdio; blocks classified as
  definitely / indirectly / possibly lost or still reachable by a
  root scan (linker-script symbols or `dl_iterate_phdr`) and a BFS
  over the block graph; `HMALLOC_REPORT_MAX` caps the blocks shown
- Optional allocation stack traces (`HMALLOC_LEAK_TRACE=1`),
  symbolized through `<debuginfo.h>`
- `HMALLOC_FT42` build profile for the 42 malloc subject: only
  `mmap` / `munmap` / `sysconf` / `getrlimit` / `write` /
  pthread are called; TINY ≤ 1 KiB in 128 KiB zones, SMALL
  ≤ 16 KiB in 2 MiB zones (≥ 100 blocks each, checked at compile
  time), every LARGE block in its own mapping; zones pre-allocated
  at load time and returned to the system as soon as they are
  empty; exact requested sizes in `show_alloc_mem`; pointer checks
  on by default

#### Debug info and dynamic linking

- `<elf.h>`: ELF64 types (`Elf64_Ehdr`, `Elf64_Shdr`, `Elf64_Sym`,
  `Elf64_Phdr`, `ElfW()`) and the `ET_*` / `EM_*` / `SHT_*` /
  `STT_*` / `STB_*` / `SHN_*` / `PT_*` / `PF_*` constants
- ELF handle (`elf_open`, `elf_open_self`, `elf_close`, section
  lookup by name or type, `elf_foreach_symbol`)
- `<link.h>`: `struct dl_phdr_info` and `dl_iterate_phdr`, built
  on `/proc/self/maps`
- `<debuginfo.h>`: symbol and source-line lookup (`di_init`,
  `di_lookup`, `di_object_find_*`, `di_object_foreach_*`,
  `di_enumerate_loaded`, ...) from `.symtab` and a DWARF 2-5
  `.debug_line` parser, with an mmap-chunk arena usable from a
  crash handler

#### C library

- `<stdatomic.h>`: `memory_order`, `atomic_*` types and
  operations, `atomic_flag`; GCC uses `__atomic_*`, Clang
  `__c11_atomic_*`
- `qsort` (pattern-defeating quicksort: in place, O(n log n)
  worst case)
- `realpath` (symlinks, `.` / `..`, `SYMLOOP_MAX` 40, `NULL`
  buffer mode)
- `getenv`, `setenv`, `unsetenv`, `putenv`
- `strerror_r`; `strdup`, `strndup`
- `getcwd`, `pipe`, `dup2`, `lseek`, `readlink`;
  `STDIN_FILENO` / `STDOUT_FILENO` / `STDERR_FILENO`, `SEEK_*`,
  raw `syscall()`
- `<sys/prctl.h>` and `prctl()` (Linux)
- `SYS_dup3` and `SYS_pipe2` on every target, `SYS_readlinkat`
  and `SYS_getcwd` where missing, `SYS_getuid` / `SYS_getgid` on
  Linux aarch64
- Minimal `<stdio.h>`
- `<bits/compiler.h>`: `HAJ_OPAQUE_PTR()`; `<bits/types.h>`:
  unaligned aliasing types `__hajMemU64_t` / `U32` / `U16`
- `HAJ_STRING_NO_DISPATCH`: `memcpy` / `memset` without the x86_64
  CPU dispatch

#### Runtime

- Per-thread TLS areas: one contiguous block holding the TLS
  variables and the TCB (variant I on aarch64, variant II on
  x86_64), with `__haj_tlsAreaSize`, `__haj_tlsAreaInit` and
  `__haj_tcbToTp`
- `<bits/symbols.h>`: the linker-script symbols in one place
- Stack-protector canary seeded from `AT_RANDOM`

#### Build, tests and benchmarks

- `make malloc`: builds `libft_malloc_$(HOSTTYPE).so` and the
  `libft_malloc.so` link for `LD_PRELOAD` (dylib on Darwin), with
  its own object tree and header dependencies
- `mk/malloc.mk`: standalone `HMALLOC_FT42` build of the 42 library
- `make SHOW_FLAGS=1` / `UNWIND=1`, `HAJ_EXTRA_CPPFLAGS` /
  `HAJ_EXTRA_CFLAGS`, `HAJ_LDSCRIPT` and `HAJ_COMMON_CFLAGS`
- `tests/standalone/malloc/`: allocator test suite run against a
  compile-time matrix (release, debug4, canary, check, encode,
  profile, nostats, paranoid, all_on), plus a `test-leak` demo
- `benchmark/malloc/`: throughput and memory benchmarks against
  glibc, jemalloc, mimalloc and tcmalloc, with a typed analyzer
  and an HTML report including a test-machine block

### Changed

- `strerror` uses a sparse table; unknown codes go to a
  thread-local buffer
- `__HAJ_THREAD_LOCAL` is always a real TLS qualifier
- `memcpy` / `memset`: no byte loops; overlapping head / tail
  stores for small sizes, aligned 4-vector main loop
- `dup2` goes through `dup3`, `pipe` through `pipe2`, `readlink`
  through `readlinkat`
- Condition variables count their waiters and skip the futex wake
  when nobody waits
- Mutexes read the caller's TID from the TCB instead of calling
  `gettid(2)`
- `pthread_create` gives new threads a full TLS area and passes
  the thread pointer as `CLONE_SETTLS`
- Build flags: LTO, `-fno-plt`, `-fno-semantic-interposition`,
  function / data sections with `--gc-sections`,
  `-march=native` (not on cross builds), `-z now` / `relro`,
  `-Bsymbolic-functions`, `-fvisibility=hidden`,
  `-mno-outline-atomics` on aarch64
- `mk/config.mk` derives its flags from `mk/hajlib.mk` and drops
  `-Werror` / `-march=native` on cross builds
- `mk/hajlibc.ld` rewritten: split `PT_LOAD` segments, sorted
  ctor / dtor arrays, section and TLS symbols, segments aligned on
  `MAXPAGESIZE`, no hardcoded output format, and an `ASSERT` on
  the main-thread TLS size
- `make fclean` also removes the preload library; an empty
  `HOSTTYPE` counts as unset

### Fixed

- `strstr` missed matches past the first ~512 bytes of the
  haystack
- New threads got a bare TCB instead of a TLS area (`.tdata` not
  copied, `.tbss` not zeroed)
- aarch64: `clone` received the TCB address instead of the thread
  pointer
- `pthread_once` woke only one waiter
- Lost wakeup window in `pthread_cond_signal` /
  `pthread_cond_broadcast`

### Removed

- Nothing

### Deprecated

- Nothing

## [0.4.0] - 2026-10-05

This release brings full POSIX threads and signals to hajlibc:
thread creation, cancellation, robust mutexes, condition
variables, read-write locks, barriers, spinlocks,
process-shared objects behind a compile-time flag, the complete
POSIX signal API, `sched.h` with CPU affinity, and the C11
`<threads.h>` compatibility layer. It also adds
`pthread_atfork`, scheduling parameters, CPU-time clocks, and
the `clocklock` / `clockrdlock` / `clockwrlock` variants from
POSIX.1-2024.

### Added

#### Threading internals

- `bits/thread/` reorganized from `bits/`: `pthreadtypes.h`,
  `pthread.h`, `clone.h`, `futex.h`, `tcb.h`, `stack.h`,
  `thread.h`, `tls.h`
- `src/thread/clone_x86_64.S`, `clone_aarch64.S`: `__haj_clone3Raw`
  and `__haj_cloneLegacyRaw` wrappers (child stack setup,
  `fn(arg)`, `SYS_exit`); libgcc atomic fallbacks on aarch64
- `src/thread/atomics_aarch64.S`: out-of-line atomics helpers
  with LSE / LL-SC runtime selection
- `src/thread/clone.c`: `__haj_clone` dispatch (clone3 with
  clone fallback, cached per process)
- `src/thread/internal/futex.c`: `__haj_futex*` wrappers with
  PRIVATE and SHARED variants, PI ops for robust mutexes
- `src/thread/internal/gettid.c`: `__haj_gettid` and
  `__haj_gettid_thread`
- `src/thread/internal/list.c`: global thread list + spinlock
- `src/thread/internal/stack.c`: bounded stack cache
  (`HAJ_STACK_CACHE_MAX`, `__haj_stackCacheFlush`)
- `src/thread/internal/start.c`: `__haj_threadStart` /
  `__haj_threadExit`
- `src/thread/internal/tcb.c`: `__haj_tcbCreate`
- `mk/sources.mk`, `mk/targets.mk`: `THREAD_SRCS`,
  `THREAD_BASE_SRCS`, per-arch clone selection

#### Public pthread API

- `pthread.h`: `pthread_create`, `_join`, `_detach`, `_self`,
  `_exit`, `_equal`, full `pthread_attr_*`
- `pthread_once` and TSD (`pthread_key_create`, `_delete`,
  `_setspecific`, `_getspecific`), with destructors running at
  thread exit
- `pthread_mutex_*` and `pthread_mutexattr_*`:
  NORMAL / RECURSIVE / ERRORCHECK, `timedlock`, robust, pshared,
  protocol, prioceiling, `clocklock`, `consistent`,
  `getprioceiling` / `setprioceiling` (return `ENOTSUP`)
- Robust mutexes via kernel PI futexes and the robust list
  (`set_robust_list`), `FUTEX_OWNER_DIED` handling,
  `pthread_mutex_consistent`
- Process-shared pthread objects behind
  `HAJ_PTHREAD_PROCESS_SHARED`
- `pthread_cond_*` (wait, timedwait, clockwait, signal,
  broadcast) and `pthread_condattr_*`
- `pthread_rwlock_*` / `pthread_rwlockattr_*`, including
  `clockrdlock` / `clockwrlock`
- `pthread_barrier_*` / `pthread_barrierattr_*`
- `pthread_spin_*`
- `pthread_cancel`, `pthread_testcancel`,
  `pthread_setcancelstate`, `pthread_setcanceltype`,
  `pthread_cleanup_push` / `pthread_cleanup_pop`,
  `PTHREAD_CANCELED`, `PTHREAD_NULL`
- `pthread_atfork` and internal atfork handlers wired into
  `fork`
- `pthread_getcpuclockid`
- `pthread_getschedparam`, `pthread_setschedparam`,
  `pthread_setschedprio`
- `pthread_setaffinity_np` / `_getaffinity_np` and
  `pthread_attr_setaffinity_np` / `_getaffinity_np`
  (GNU extensions)

#### Signals

- `signal.h` with the full POSIX signal API (`sigaction`,
  `signal`, `sigprocmask`, `sigpending`, `sigsuspend`,
  `sigaltstack`, `sigset_*`)
- `raise`, `kill`, `tgkill`, `killpg`, `sigqueue`,
  `sigtimedwait`, `sigwaitinfo`, `sigwait`, `pthread_kill`,
  `pthread_sigmask`
- `sig2str` / `str2sig` / `strsignal` / `psignal` / `psiginfo`
- x86_64 `rt_sigreturn` trampoline (`sa_restorer`)
- `bits/signal.h`: per-OS signal numbers, `SA_*` flags, `si_code`

#### Scheduling

- `sched.h`: policies (`SCHED_OTHER` / `FIFO` / `RR` / `BATCH`
  / `IDLE` / `DEADLINE`), `struct sched_param`,
  `sched_yield`, `sched_{get,set}scheduler`,
  `sched_{get,set}param`, `sched_get_priority_{min,max}`,
  `sched_rr_get_interval`, `sched_{get,set}affinity`
- `bits/cpuSet.h`: `cpu_set_t`, `CPU_ZERO` / `SET` / `CLR` /
  `ISSET`, and `__haj_cpuCount` / `And` / `Or` / `Xor` / `Equal`
- `src/sched/cpuSet.c`, `src/sched/sched.c`

#### C11 threads

- `<threads.h>`: `thrd_create` / `_current` / `_equal` /
  `_sleep` / `_yield` / `_detach` / `_join` / `_exit`,
  `mtx_*`, `cnd_*`, `tss_*`, `call_once`, `once_flag`,
  `thread_local`, `TSS_DTOR_ITERATIONS`

#### Other additions

- `sys/wait.h` (`wait`, `waitpid`, `W*` macros) and
  `src/sys/waitpid.c`
- `unistd.h`: `usleep`, `unlink`, `getppid`, `gettid`,
  `getuid`, `getgid`
- `useconds_t` in `bits/types.h`
- Constructors/destructors: `__haj_run_ctors` /
  `__haj_run_dtors` (called by `_start` and `exit`)
- `mk/hajlibc.ld`: minimal linker script exposing `.init_array`
  / `.fini_array` boundaries
- Test framework in `tests/framework/` (`TEST`, `ASSERT`,
  `ASSERT_EQ`, `ASSERT_STREQ`, colored report, filter)
- Test cases: `ctype` (14 tests), `thread` (create, join,
  detach, once, key, mutex, cond, rwlock, barrier, spin,
  futex, clone, cancel, attr, pthread)
- `make cov-build` / `make cov-clean` in the root Makefile
- `HAJ_COV=1` support in `mk/config.mk`

### Changed

- `bits/types.h` includes `bits/thread/pthreadtypes.h`
  (removed inline pthread typedef placeholders)
- Opaque pthread types are now defined in
  `bits/thread/pthreadtypes.h` with size/alignment
  `_Static_assert` checks
- `bits/cpuSet.h` extracted from `tcb.h`
- `src/crt/` split into `start/` (per-OS entry + C startup)
  and `exit/` (`exit`, `_exit`, `abort`, `atexit`, `cxa_*`)
- `_start` is now a thin asm stub passing `sp` and `&main`
  to `__hajlibcStartMain`
- `__dso_handle` moved to `src/crt/start/start.c`
- `mk/targets.mk`: `CRT_START_SRCS` paths updated
  (`src/crt/start/<os>/...`)
- `abort` now uses `raise(SIGABRT)` with proper unblock and
  reset
- `src/unistd/getids.c` replaces `getpid.c`
- `mk/hajlib.mk`: added `-T $(HAJ_ROOT)/hajlibc.ld`
- `memcpy` / `memmove` / `memset` dispatchers use typed
  function pointers instead of `void *`
- `mk/cross.mk`: `CC` now uses `?=` so the user can override
  the compiler
- `mk/build.mk`: `.c` and `.S` compile rules include
  `$(COV_CFLAGS)`
- `tests/main.c` now runs the test framework instead of
  printing a version
- CPU detection split per-arch: `src/cpu/detect.c`,
  `features_x86_64.c`, `features_aarch64.c`
- `hajCpuHas*()` accessors no longer call detection lazily;
  detection runs once at startup from `hajCpuDetect()`
- aarch64: `hajCpuHasLse()`, `hajCpuHasNeon()`,
  `__haj_cpuHasLse` for atomics selection
- C-side CRT startup: `__hajlibcStartMain` (parses initial
  stack, sets up globals, CPU detection, TCB, ctors, calls
  main)
- `src/crt/start/tcb.c`: `__hajSetThreadRegister` installs
  the main thread TCB
- Renamed of hajlib in License header to `hajlibc` to match
  the library name

### Fixed

- `pthread_create`: handle `attr == NULL` without dereferencing
  an uninitialized `_hajThreadAttr *`
- `pthread_create`: remove the TCB from the global list on
  clone failure so it is not left as a ghost entry
- `pthread_create`: apply `PTHREAD_EXPLICIT_SCHED` policy in
  the new thread's trampoline instead of mutating the caller's
  `pthread_attr_t`
- `pthread_rwlock_unlock`: CAS loop for the writer release so
  a concurrent `WRWAIT` is not lost
- `pthread_rwlock_unlock`: wake all waiters when the last
  reader releases and a writer is waiting (previous
  `wake(1)` could wake a reader and starve the writer)
- `pthread_barrier_wait`: compare `add_fetch` result to
  `total` (not `total - 1`) so the last thread is correctly
  detected
- `sigfillset`: no longer clears `SIGHUP`
- `sigaction`: accept `SIGRTMAX` (`>= HAJ_NSIG` was rejecting
  it)
- `pthread_kill`: return the real errno instead of `EPERM`
- `pthread_getschedparam` / `setschedparam` / `setschedprio`:
  return `errno` instead of hard-coded `ESRCH`
- `pthread_create`: register the atexit cache flush handler
  only after a successful clone

### Removed

- `pthread.h`: duplicate `struct sched_param` definition
  (now taken from `sched.h`)
- `bits/thread/thread.h`: `HAJ_SCHED_*` macros (replaced by
  `SCHED_*` from `sched.h`)
- `src/unistd/getpid.c` (merged into `src/unistd/getids.c`)

### Deprecated

- Nothing yet

## [0.3.0] - 2026-09-28

This release adds the full string and ctype APIs, the memory
mapping interface, the random API, and the complete time API.
Programs can now use `memcpy`, `strlen`, `mmap`, `getrandom`,
`clock_gettime`, `nanosleep`, and the POSIX timers without
falling back to the system libc.

### Added

#### String and character handling

- `<string.h>`: `memcpy`, `memmove`, `memset` with SSE2 / AVX2 /
  AVX-512 / NEON dispatch and a portable fallback
- `<string.h>`: `memcmp`, `memchr`, `memccpy`, `memrchr`
- `<string.h>`: `strlen`, `strnlen` (word-at-a-time scan)
- `<string.h>`: `strcmp`, `strncmp` (word-at-a-time comparison)
- `<string.h>`: `strchr`, `strrchr`, `strchrnul`, `strcpy`,
  `stpcpy`, `strncpy`, `stpncpy`, `strlcpy`
- `<string.h>`: `strcat`, `strncat`, `strlcat`, `strspn`, `strcspn`
- `<string.h>`: `strpbrk`, `strstr` (Horspool + Two-Way),
  `strcasestr`
- `<string.h>`: `strtok`, `strtok_r`, `strsep`
- `<ctype.h>`: full character classification and conversion
  (inline `__haj_*` fast paths, external symbols in
  `src/ctype/ctype.c`)

#### Memory mapping

- `<sys/mman.h>`: `mmap`, `munmap`, `mprotect`, `msync`,
  `madvise`, `posix_madvise`
- `<sys/mman.h>`: `mlock`, `munlock`, `mlockall`, `munlockall`
- `<sys/mman.h>`: `shm_open`, `shm_unlink` (per-OS: `/dev/shm`
  on Linux, native syscalls on FreeBSD, `/var/tmp/.hajlib-shm-*`
  on Darwin)
- `<sys/mman.h>`: POSIX typed memory API
- `<bits/mman.h>`: per-OS `PROT_*`, `MAP_*`, `MS_*`, `MADV_*`,
  `MCL_*` constants
- `<bits/mman.h>`: `SHM_PREFIX`, `SHM_PREFIX_LEN`, `SHM_PATH_MAX`

#### Random

- `<sys/random.h>`: `getrandom` (with `GRND_*` flags) and
  `getentropy`

#### Time

- `<time.h>`: complete ISO C + POSIX time API
  - `<time.h>`: `struct tm` (with `tm_gmtoff`, `tm_zone`),
    `struct itimerspec`
  - `<time.h>`: `time`, `difftime`, `timespec_get`, `clock`
  - `<time.h>`: `clock_gettime`, `clock_getres`, `clock_settime`,
    `clock_getcpuclockid`
  - `<time.h>`: `nanosleep`, `clock_nanosleep`
  - `<time.h>`: `timer_create`, `timer_delete`, `timer_gettime`,
    `timer_settime`, `timer_getoverrun` (declarations only)
  - `<time.h>`: `gmtime`, `gmtime_r`, `localtime`, `localtime_r`,
    `mktime`
  - `<time.h>`: `strftime`, `strptime`, `asctime`, `ctime`,
    `getdate` (declarations only)
  - `<time.h>`: `tzset`, `daylight`, `timezone`, `tzname`,
    `getdate_err`
- `<sys/time.h>`: `struct timeval`, `select`, `utimes`
- `<sys/times.h>`: `struct tms`, `times`
- `<bits/time.h>`: `struct timespec`, `struct timeval`, per-OS
  `CLOCK_*`, `CLOCKS_PER_SEC`, `TIMER_ABSTIME`, `TIME_UTC`

#### Signal (partial)

- `<signal.h>`: `union sigval`, `struct sigevent` (the minimum
  required for POSIX timer notification; the full signal API is
  not yet implemented)

#### Type system

- `<bits/types.h>`: `id_t`, `key_t`, `fsblkcnt_t`, `fsfilcnt_t`,
  `reclen_t`
- `<bits/types.h>`: opaque `timer_t`, `pthread_t`,
  `pthread_key_t`, `pthread_once_t`, `pthread_spinlock_t`,
  `pthread_attr_t`, `pthread_barrier_t`,
  `pthread_barrierattr_t`, `pthread_cond_t`,
  `pthread_condattr_t`, `pthread_mutex_t`,
  `pthread_mutexattr_t`, `pthread_rwlock_t`,
  `pthread_rwlockattr_t`
- `<bits/select.h>`: `fd_set`, `FD_ZERO`, `FD_SET`, `FD_CLR`,
  `FD_ISSET`
- `<bits/wordsize.h>`: `__HAJ_USE_32_OFFSET_BITS` (32-bit `off_t`
  only when the platform is 32-bit and `_FILE_OFFSET_BITS=32`)

#### Syscall layer

- `__haj_syscall0` .. `__haj_syscall5` variants for all supported
  OS/arch
- `SYS_time` added for Linux x86_64/aarch64 and FreeBSD
- All syscall assembly files now emit `.note.GNU-stack`

#### Build

- `Makefile`: `init`, `headers-add`, `headers-check` targets
- `mk/sources.mk`: `SYS_DIR` and `SYS_SRCS` (`getentropy`,
  `getrandom`, `times`), `TIME_SRCS` for `src/time/`

### Changed

- Public POSIX typedefs (`size_t`, `ssize_t`, `off_t`, `pid_t`,
  `time_t`, ...) moved from `<sys/types.h>` to `<bits/types.h>`;
  `<sys/types.h>` is now just `#include <bits/types.h>`
- Renamed library references from `libhaj.a` to `libhajc.a`
  (`mk/config.mk`, `mk/hajlib.mk`)
- Internal syscall call sites rewritten to use the smallest
  `__haj_syscallN` variant
- `mk/targets.mk` uses `SYSCALL_BASE_SRCS` to include all 7
  syscall files per target

### Fixed

- Nothing yet

### Removed

- Nothing yet

### Deprecated

- `<time.h>`: `asctime`, `ctime` are marked obsolescent in
  POSIX.1-2024 and tagged `__HAJ_DEPRECATED`
- `<time.h>`: `daylight`, `timezone`, `tzname` are XSI and
  obsolete; prefer `tm_gmtoff`, `tm_zone`, and `localtime_r`

## [0.2.0] - 2026-09-22

This is the first version with a working C runtime, syscall
layer, and standard type system. It is not yet a complete libc:
`malloc`, `printf`, and most of the string functions and header
files are still missing. But the foundation is solid: a program
can be compiled against hajlib, linked statically, and run on
Linux (x86_64 and aarch64) without any dependency on the system libc.

### Added

#### Type system and standard headers

- Compiler abstraction: `include/bits/compiler.h` with detection
  of GCC, Clang, and MSVC, plus portable macros for `noreturn`,
  `unused`, `packed`, `aligned`, `weak`, `alias`, `used`,
  `constructor`, `destructor`, `likely`, `unlikely`, `pure`,
  `const`, `printf`, `scanf`, `malloc`, `deprecated`, `inline`,
  `restrict`, `alignof`, and `static_assert`
- OS detection: `include/bits/os.h` (`HAJ_OS_LINUX`,
  `HAJ_OS_FREEBSD`, `HAJ_OS_DARWIN`, `HAJ_OS_WINDOWS`)
- Architecture detection: `include/bits/arch.h`
  (`HAJ_ARCH_X86_64`, `HAJ_ARCH_AARCH64`, `HAJ_ARCH_I386`,
  `HAJ_ARCH_ARM`)
- Word size and endianness: `include/bits/wordsize.h`
  (`__HAJ_WORDSIZE`, `__HAJ_BYTE_ORDER`, `__HAJ_SIZEOF_*`)
- Internal and POSIX types: `include/bits/types.h`,
  `include/sys/types.h` (`size_t`, `ssize_t`, `off_t`,
  `mode_t`, `pid_t`, `uid_t`, `gid_t`, `time_t`, ...)
  with per-OS ABI matching
- Standard headers: `include/stddef.h`, `include/stdint.h`,
  `include/stdbool.h`, `include/stdarg.h`, `include/limits.h`
  with `include/bits/limits.h`
- `<sys/auxv.h>`: `getauxval` and the `AT_*` constants
- `<unistd.h>`: `getpid`

#### Error handling

- Per-OS error codes: `include/bits/errno.h`
- Public errno: `include/errno.h`, `src/errno/errno.c`
  (thread-local, but TLS is not yet enabled)
- `strerror()` and `strerror_r()` with per-OS message tables
- Unknown error codes are formatted into a thread-local buffer

#### C runtime (CRT)

- Startup code for Linux x86_64 / aarch64, FreeBSD x86_64 /
  aarch64, and Darwin x86_64 / arm64
- `_start` reads `argc`/`argv`/`envp`, stores `argv[0]`, `__progname`,
  `__haj_argc`, `environ`, and `__haj_auxv`, calls `main`, then `exit`
- `exit`, `_exit`, `abort`
- `atexit`, `__cxa_atexit`, `__cxa_finalize`, `__dso_handle`
- `include/bits/crt.h` (internal declarations)

#### Syscall layer

- Raw syscall primitive: `__haj_syscall6` in
  `include/bits/syscall.h`
- Per-OS/arch syscall numbers in `include/bits/syscall/`
- Per-OS/arch implementations:
  - Linux x86_64 / aarch64 (raw Linux convention)
  - FreeBSD x86_64 / aarch64 (carry-flag error normalized)
  - Darwin x86_64 / arm64 (same as FreeBSD)
  - Windows: C shim dispatching to the mingw CRT

#### File control

- Flags and commands: `include/bits/fcntl.h`
  (`O_*`, `F_*`, `FD_CLOEXEC`, `struct flock`, per-OS)
- Public header: `include/fcntl.h` (`open`, `creat`, `fcntl`)
- Implementations: `src/fcntl/open.c` (uses `SYS_openat` with
  `AT_FDCWD`), `src/fcntl/creat.c`, `src/fcntl/fcntl.c`
  (variadic, argument-type dispatch per command)

#### Assertions

- `include/assert.h`, `src/assert/assert.c`
- `__assert_fail`, `__assert_perror_fail`, and the `assert`
  macro (disabled when `NDEBUG` is defined)

#### Non-local jumps

- `include/setjmp.h`, `src/setjmp/{x86_64,aarch64}/`
- `setjmp`, `longjmp`, `_setjmp`, `_longjmp`, `sigsetjmp`,
  `siglongjmp`
- Signal mask save/restore is a stub until `sigprocmask` is
  implemented

#### Stack protection

- `src/stack_chk/stack_chk_guard.c` (`__stack_chk_guard`)
- `src/stack_chk/stack_chk_fail.c` (`__stack_chk_fail`,
  `__stack_chk_fail_local`)
- The canary is currently a fixed value; it will be randomized
  with `getrandom()` once the random subsystem is available

#### Compiler runtime helpers

- `src/runtime/div.c` (`__udivdi3`, `__umoddi3`, `__divdi3`,
  `__moddi3`)
- `src/runtime/mul.c` (`__muldi3`)
- These are only used on 32-bit targets; on x86_64 and aarch64,
  the compiler uses native instructions

#### Build system

- `mk/config.mk` exposes public `HAJ_*` variables and a
  `HAJ_COMMON_CFLAGS` variable, so external projects can build
  against hajlib without knowing the internal flags
- `mk/targets.mk` selects OS/arch-specific sources
- `mk/hajlib.mk` is a standalone include for external projects
- `make test` builds the library and runs the test programs
- `tests/` contains a minimal link test

### Changed

- Build files moved from the repository root to `mk/`
- The archive is now ordered low-level-first so the linker can
  resolve symbols in a single pass

### Fixed

- `.gitignore` now ignores `tests/build/`

### Removed

- Removed the per-subsystem `.mk` files at the root
  (`build.mk`, `cross.mk`, `sources.mk`, `version.mk`)
- Removed `syscall.mk` (merged into `mk/targets.mk`)

### Deprecated

- Nothing yet

## [0.1.0] - 2026-09-21

### Added

- Initial project structure: `include/`, `src/`, `test/`
- Build infrastructure: `Makefile`, `build.mk`, `sources.mk`,
  `cross.mk`, `syscall.mk`
- Versioning: `version.mk`, `include/haj/version.h`
- Release script: `scripts/release.sh`
- Development tooling: `.editorconfig`, `.clangd`,
  `.clang-tidy`, `.gitignore`, `.gitattributes`

### Changed

- Rewrote the library from a libft-based structure to a
  POSIX-like libc structure

### Removed

- All `ft_*` sources and headers from the old libft structure

[Unreleased]: https://github.com/moutigll/hajlib/compare/v0.5.0...dev
[0.5.0]: https://github.com/moutigll/hajlib/compare/v0.4.0...v0.5.0
[0.4.0]: https://github.com/moutigll/hajlib/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/moutigll/hajlib/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/moutigll/hajlib/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/moutigll/hajlib/releases/tag/v0.1.0
