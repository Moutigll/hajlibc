# sources.mk - list of source files for hajlib.
#
# Each section corresponds to a public header or a subsystem. The
# section names are used by the Makefile to build object file
# paths.
#
# Rules:
#   - Each section lists .c and .S files relative to its directory.
#   - The Makefile prefixes with the section directory.
#   - Use tab indentation, one file per line, with trailing backslash.
#   - Sections are ordered alphabetically by directory name.
#   - OS/arch-specific sources (CRT start, syscall, setjmp) are
#     selected in targets.mk, not here.

# Section directories
SRC_DIR	:= src

ASSERT_DIR		:= $(SRC_DIR)/assert
CPU_DIR			:= $(SRC_DIR)/cpu
CRT_DIR			:= $(SRC_DIR)/crt
CTYPE_DIR		:= $(SRC_DIR)/ctype
ERRNO_DIR		:= $(SRC_DIR)/errno
FCNTL_DIR		:= $(SRC_DIR)/fcntl
GETOPT_DIR		:= $(SRC_DIR)/getopt
MATH_DIR		:= $(SRC_DIR)/math
MMAN_DIR		:= $(SRC_DIR)/sys/mman
RUNTIME_DIR		:= $(SRC_DIR)/runtime
SETJMP_DIR		:= $(SRC_DIR)/setjmp
SIGNAL_DIR		:= $(SRC_DIR)/signal
STACK_CHK_DIR	:= $(SRC_DIR)/stack_chk
STAT_DIR		:= $(SRC_DIR)/stat
STDIO_DIR		:= $(SRC_DIR)/stdio
STDLIB_DIR		:= $(SRC_DIR)/stdlib
STRING_DIR		:= $(SRC_DIR)/string
SYS_DIR			:= $(SRC_DIR)/sys
TIME_DIR		:= $(SRC_DIR)/time
UNISTD_DIR		:= $(SRC_DIR)/unistd

# assert (portable C parts)
ASSERT_SRCS := \
	assert.c

# cpu (portable C parts)
CPU_SRCS :=

# crt (portable C parts)
CRT_SRCS := \
	_exit.c \
	atexit.c \
	cxa_atexit.c \
	cxa_finalize.c \
	dso_handle.c \
	exit.c \
	start.c

# ctype
CTYPE_SRCS := \
	ctype.c

# errno
ERRNO_SRCS := \
	errno.c
#	strerror.c  (needs snprintf, not implemented yet)

# fcntl
FCNTL_SRCS := \
	creat.c \
	fcntl.c \
	open.c

# getopt--
GETOPT_SRCS :=

# math
MATH_SRCS :=

# mman---
MMAN_SRCS := \
	mmap.c \
	munmap.c \
	mlock.c \
	munlock.c \
	mlockall.c \
	munlockall.c \
	mprotect.c \
	msync.c \
	madvise.c \
	posix_typed_mem.c

# runtime (libgcc helpers)
RUNTIME_SRCS := \
	div.c \
	mul.c

# setjmp (portable C parts)
SIGSETJMP_SRCS := \
	sigsetjmp.c

# signal
SIGNAL_SRCS :=

# stack_chk
STACK_CHK_SRCS := \
	stack_chk_fail.c \
	stack_chk_guard.c

# stat
STAT_SRCS :=

# stdio
STDIO_SRCS :=

# stdlib-
STDLIB_SRCS :=

# string
STRING_SRCS := \
	mem/memcpy.c \
	mem/memmove.c \
	mem/memset.c \
	mem/memcmp.c \
	mem/memchr.c \
	mem/memrchr.c \
	mem/memccpy.c \
	str/strlen.c \
	str/strnlen.c \
	str/strcmp.c \
	str/strncmp.c \
	str/strchr.c \
	str/strrchr.c \
	str/strchrnul.c \
	str/strcpy.c \
	str/stpcpy.c \
	str/strncpy.c \
	str/stpncpy.c \
	str/strlcpy.c \
	str/strcat.c \
	str/strncat.c \
	str/strlcat.c \
	str/strspn.c \
	str/strcspn.c \
	str/strpbrk.c \
	str/strstr.c \
	str/strcasestr.c \
	str/strtok.c \
	str/strtok_r.c \
	str/strsep.c

SYS_SRCS := \
	auxv.c \
	getentropy.c \
	getrandom.c \
	times.c

# time
TIME_SRCS := \
	time.c \
	timespec_get.c \
	nanosleep.c \
	clock/clock_getres.c \
	clock/clock_gettime.c \
	clock/clock_nanosleep.c \
	clock/clock_settime.c

# unistd-
UNISTD_SRCS := \
	getpid.c

# Prefix each section with its directory.
ASSERT_SRCS		:= $(addprefix $(ASSERT_DIR)/,$(ASSERT_SRCS))
CPU_SRCS		:= $(addprefix $(CPU_DIR)/,$(CPU_SRCS))
CRT_SRCS		:= $(addprefix $(CRT_DIR)/,$(CRT_SRCS))
CTYPE_SRCS		:= $(addprefix $(CTYPE_DIR)/,$(CTYPE_SRCS))
ERRNO_SRCS		:= $(addprefix $(ERRNO_DIR)/,$(ERRNO_SRCS))
FCNTL_SRCS		:= $(addprefix $(FCNTL_DIR)/,$(FCNTL_SRCS))
GETOPT_SRCS		:= $(addprefix $(GETOPT_DIR)/,$(GETOPT_SRCS))
MATH_SRCS		:= $(addprefix $(MATH_DIR)/,$(MATH_SRCS))
MMAN_SRCS		:= $(addprefix $(MMAN_DIR)/,$(MMAN_SRCS))
RUNTIME_SRCS	:= $(addprefix $(RUNTIME_DIR)/,$(RUNTIME_SRCS))
SIGSETJMP_SRCS	:= $(addprefix $(SETJMP_DIR)/,$(SIGSETJMP_SRCS))
SIGNAL_SRCS		:= $(addprefix $(SIGNAL_DIR)/,$(SIGNAL_SRCS))
STACK_CHK_SRCS	:= $(addprefix $(STACK_CHK_DIR)/,$(STACK_CHK_SRCS))
STAT_SRCS		:= $(addprefix $(STAT_DIR)/,$(STAT_SRCS))
STDIO_SRCS		:= $(addprefix $(STDIO_DIR)/,$(STDIO_SRCS))
STDLIB_SRCS		:= $(addprefix $(STDLIB_DIR)/,$(STDLIB_SRCS))
STRING_SRCS		:= $(addprefix $(STRING_DIR)/,$(STRING_SRCS))
SYS_SRCS		:= $(addprefix $(SYS_DIR)/,$(SYS_SRCS))
TIME_SRCS		:= $(addprefix $(TIME_DIR)/,$(TIME_SRCS))
UNISTD_SRCS		:= $(addprefix $(UNISTD_DIR)/,$(UNISTD_SRCS))
