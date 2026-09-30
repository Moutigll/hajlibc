# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Constructors/destructors: `__haj_run_ctors` / `__haj_run_dtors` (called by `_start` and `exit`)
- `mk/hajlibc.ld`: minimal linker script exposing `.init_array` / `.fini_array` boundaries
- `unistd.h`: `write`
- Test framework in `tests/framework/` (`TEST`, `ASSERT`, `ASSERT_EQ`, `ASSERT_STREQ`, colored report, filter)
- First test case file: `tests/cases/ctype.c` (14 tests)
- `make cov-build` / `make cov-clean` in the root Makefile
- `HAJ_COV=1` support in `mk/config.mk` (adds `--coverage`, `COV_CFLAGS`, `COV_LDLIBS`)
- Threading internals (headers only):
- `bits/signal.h`: per-OS signal numbers and `SA_*` flags
- `bits/clone.h`: Linux `clone()` flags and `HAJ_CLONE_THREAD_FLAGS`
- `bits/futex.h`: Linux futex operations, modifiers, and `__haj_futex*` wrappers
- `bits/tcb.h`: Thread Control Block layout and `__haj_tcbSelf`
- `bits/thread.h`: atomics, CPU relax, `__haj_gettid`, thread list, `cpu_set_t`
- `bits/tls.h`: TLS block layout, alloc/free, thread-area helpers
- `sys/resource.h`: `getrlimit`, `setrlimit`, `getrlimit64`, `setrlimit64`, `getrusage`, `getpriority`, `setpriority`
- `bits/resource.h`: per-OS `RLIMIT_*`, `RLIM_INFINITY`, `RLIM_SAVED_*`, `RUSAGE_*`
- `rlim_t` / `rlim64_t` in `bits/types.h` (32-bit `rlim_t` only when `__HAJ_USE_32_OFFSET_BITS`)
- `SYS_getrlimit`, `SYS_setrlimit`, `SYS_getrusage`, `SYS_prlimit64` in Linux x86_64/aarch64 syscall tables

### Changed

- Renamed of hajlib in License header to `hajlibc` to match the library name
- All `_start` entry points call `__haj_run_ctors()` before `main()`
- `mk/hajlib.mk`: added `-T $(HAJ_ROOT)/hajlibc.ld`
- `memcpy` / `memmove` / `memset` dispatchers use typed function pointers instead of `void *`
- `mk/cross.mk`: `CC` now uses `?=` so the user can override the compiler
- `mk/build.mk`: `.c` and `.S` compile rules include `$(COV_CFLAGS)`
- `tests/main.c` now runs the test framework instead of printing a version

### Fixed

- Nothing yet

### Removed

- Nothing yet

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

[Unreleased]: https://github.com/moutigll/hajlib/compare/v0.3.0...dev
[0.3.0]: https://github.com/moutigll/hajlib/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/moutigll/hajlib/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/moutigll/hajlib/releases/tag/v0.1.0
