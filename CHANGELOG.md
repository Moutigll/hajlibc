# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Development tooling: `.vscode/settings.json`, `.vscode/extensions.json`
- License header scripts: `scripts/header.sh`, `scripts/header-update.sh`, `scripts/header-check.sh`
- Git hook: `.githooks/pre-commit` (checks headers on commit)
- CPU feature detection: `include/bits/cpu.h` (SSE2/3/SSSE3, SSE4.1/4.2, AVX, AVX2, FMA, AVX-512F/BW/VL, ERMS)
- Implementation: `src/cpu/x86/cpuFeatures.c` (CPUID + XGETBV, lazy detection, cached in static struct)
- Compiler attributes: `__HAJ_MAY_ALIAS`, `__HAJ_EXTENSION`, `__HAJ_TYPEOF`, `__HAJ_ALIGNED_MAX`, `__HAJ_UNALIGNED_WORD`, `__HAJ_TARGET`, `__HAJ_IFUNC`
- Renamed library references from `libhaj.a` to `libhajc.a` (`mk/config.mk`, `mk/hajlib.mk`)
- `Makefile`, `mk/sources.mk` and `mk/targets.mk`: added CPU source selection per OS/arch
- `string.h` with `memcpy` (SSE2/AVX2/AVX-512/NEON dispatch via CPU detection, generic fallback)
- `string.h`: `memmove`
- Refactored `memcpy` into a shared vector template (`src/string/memcpyImpl.h`), reused by `memmove`
- `string.h`: `memset` (SSE2/AVX2/AVX-512/NEON dispatch) and `bzero` macro
- `__HAJ_SOURCE` macro in `bits/compiler.h` (gated on `_GNU_SOURCE`/`_BSD_SOURCE`/`_POSIX_C_SOURCE`/GNU-like compilers)
- `string.h`: `memcmp`, `memchr`, `memccpy`, and `memrchr` (GNU, under `__HAJ_SOURCE`)
- Internal helpers: `src/string/impl/wordOps.h` (`__hajSplatByte`, `__hajHasZeroByte`, first/last zero byte index)
- `string.h`: `strlen`, `strnlen` (word-at-a-time scan)
- `string.h`: `strcmp`, `strncmp` (word-at-a-time, with zero-byte/diff detection)
- `string.h`: `strchr`, `strrchr`, `strchrnul` (GNU), `strcpy`, `stpcpy`, `strncpy`, `stpncpy`, `strlcpy` (BSD)
- `string.h`: `strcat`, `strncat`, `strlcat` (BSD), `strspn`, `strcspn`
- `ctype.h`: character classification and conversion (inline `__haj_*` fast paths, `HAJ_NO_CTYPE_MACROS` toggle, external symbols in `src/ctype/ctype.c`)
- `string.h`: `strpbrk`, `strstr` (Horspool + Two-Way for long needles), `strcasestr` (GNU)
- `string.h`: `strtok`, `strtok_r`, `strsep` (BSD)
- `bits/mman.h`: per-OS `mmap` constants (`PROT_*`, `MAP_*`, `MS_*`, `MADV_*`, `MCL_*`) for Linux, FreeBSD, Darwin
- `__haj_syscall0` .. `__haj_syscall5` variants for all supported OS/arch (assembly)
- `sys/mman.h`: `mmap`, `munmap` (other mman functions declared, implementations pending)
- `src/mman/`: `mmap.c`, `munmap.c` using raw syscalls
- `sys/mman.h`: `mprotect`, `msync`, `madvise`, `posix_madvise`, `mlock`, `munlock`, `mlockall`, `munlockall`, `shm_open`, `shm_unlink`, POSIX typed-memory API
- `src/sys/mman/`: platform-agnostic implementations + per-OS `shm_open`/`shm_unlink`
- Linux: `/dev/shm` + name; FreeBSD: dedicated syscalls; Darwin: `/var/tmp/.hajlib-shm-<uid>-...`
- FreeBSD: `SYS_shm_open`, `SYS_shm_unlink` in `bits/syscall/freebsd.h`
- `bits/mman.h`: `SHM_PREFIX`, `SHM_PREFIX_LEN`, `SHM_PATH_MAX` per OS
- `sys/random.h`: `getrandom` (with `GRND_*` flags) and `getentropy`
- `src/sys/getrandom.c`, `src/sys/getentropy.c` (Linux/BSD via `SYS_getrandom`, Darwin via `SYS_getentropy`)

### Changed

- `.gitignore`: track `.vscode/settings.json` and `.vscode/extensions.json`
- `Makefile`: added `init`, `headers-add`, `headers-check` targets
- Renamed library references from `libhaj.a` to `libhajc.a` (`mk/config.mk`, `mk/hajlib.mk`)
- Rewrote internal syscall call sites to use the smallest `__haj_syscallN` variant
- `mk/targets.mk`: use `SYSCALL_BASE_SRCS` to include all 7 syscall files per target
- All syscall6 assembly files now emit `.note.GNU-stack`

### Fixed

- Nothing yet

### Removed

- Nothing yet

### Deprecated

- Nothing yet

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

#### Error handling

- Per-OS error codes: `include/bits/errno.h`
- Public errno: `include/errno.h`, `src/errno/errno.c`
  (thread-local, but TLS is not yet enabled)
- `strerror()` and `strerror_r()` with per-OS message tables
- Unknown error codes are formatted into a thread-local buffer

#### C runtime (CRT)

- Startup code for Linux x86_64 / aarch64, FreeBSD x86_64 /
  aarch64, and Darwin x86_64 / arm64
- `_start` reads `argc`/`argv`/`envp`, stores `argv[0]` into
  `__progname`, calls `main`, then `exit`
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

[Unreleased]: https://github.com/moutigll/hajlib/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/moutigll/hajlib/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/moutigll/hajlib/releases/tag/v0.1.0
