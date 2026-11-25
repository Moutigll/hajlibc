# hajlibc

**A C standard library written from scratch, for Linux on x86_64 and aarch64.**

[![version](https://openhaj.moutig.sh/assets/hajlibc/version.svg)](https://openhaj.moutig.sh/projects/hajlibc/)
[![POSIX.1-2024](https://openhaj.moutig.sh/assets/hajlibc/posix.svg)](https://openhaj.moutig.sh/projects/hajlibc/posix/)
[![tests](https://openhaj.moutig.sh/assets/hajlibc/tests.svg)](https://openhaj.moutig.sh/projects/hajlibc/tests/)
[![coverage](https://openhaj.moutig.sh/assets/hajlibc/coverage.svg)](https://openhaj.moutig.sh/projects/hajlibc/coverage/)

</div>

## About

hajlibc is a C library built from the ground up, in C and a little assembly.
It does not sit on top of another libc: it talks to the Linux kernel directly
through its own system call layer, and it has its own program startup,
thread-local storage and memory allocator.

The goal is conformance to **POSIX.1-2024**. It is checked by
[blåsix](https://github.com/openhaj/blasix), a test suite written from the
standards themselves.
