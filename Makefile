# hajlibc - Makefile
#
# A POSIX-like C library, no libc dependency, multi-OS multi-arch.
#
# Targets:
#   all           build libhajc.a (default)
#   clean         remove object files
#   fclean        remove object files and the library
#   re            fclean + all
#   version       print the current version
#   info          print build configuration
#   help          this message
#   cov-build     build with coverage instrumentation
#   cov-clean     remove coverage artifacts
#   headers-add   add/update SPDX headers in source files
#   headers-check verify SPDX headers
#   init          install git hooks
#
# Cross-compilation:
#   make WIN64=1    compile for 64-bit Windows (mingw)
#   make WIN32=1    compile for 32-bit Windows (mingw)
#   make NATIVE=1   force native compilation
#
# Optional build switches:
#   make SHOW_FLAGS=1   print every compilation command
#   make UNWIND=1       one progress line per file (no \r overwrite)
#   make HAJ_COV=1      coverage build
#   make HAJ_PTHREAD_PROCESS_SHARED=1   enable process-shared mutexes

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
# config.mk includes cross.mk and hajlib.mk. It defines:
#   toolchain (CC, AR, LD)
#   target   (TARGET_OS, TARGET_ARCH, TARGET_EXT, CROSS_COMPILING)
#   internal flags (CPPFLAGS, CFLAGS, LDFLAGS, LDLIBS)
#   coverage flags (COV_*)
#   public paths (HAJ_ROOT, HAJ_INCLUDE, HAJ_LIB, HAJ_LDSCRIPT)

HAJ_ROOT := $(CURDIR)
include mk/config.mk

# ----- Version, sources, target-specific sources, build rules -----
include mk/version.mk
include mk/sources.mk
include mk/targets.mk
include mk/build.mk

# ----- Library -----
NAME	:= libhajc.a
OBJDIR	:= objs

# Source order matters for a static archive: the linker scans left to
# right, so low-level modules (runtime, syscalls, crt) go first and
# high-level modules (stdio, malloc, stdlib) last.
ALL_SRCS := \
	$(CPU_SRCS) \
	$(CRT_SRCS) \
	$(CRT_START_SRCS) \
	$(DEBUGINFO_SRCS) \
	$(SYS_SRCS) \
	$(SYSCALL_SRCS) \
	$(SETJMP_SRCS) \
	$(SIGSETJMP_SRCS) \
	$(RUNTIME_SRCS) \
	$(STACK_CHK_SRCS) \
	$(ASSERT_SRCS) \
	$(ERRNO_SRCS) \
	$(STRING_SRCS) \
	$(FCNTL_SRCS) \
	$(CTYPE_SRCS) \
	$(SCHED_SRCS) \
	$(STDLIB_SRCS) \
	$(MALLOC_SRCS) \
	$(STDIO_SRCS) \
	$(MATH_SRCS) \
	$(THREAD_SRCS) \
	$(THREAD_BASE_SRCS) \
	$(TIME_SRCS) \
	$(SIGNAL_SRCS) \
	$(UNISTD_SRCS) \
	$(STAT_SRCS) \
	$(MMAN_SRCS) \
	$(MMAN_PLATFORM_SRCS) \
	$(GETOPT_SRCS)

ALL_OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(patsubst %.S,$(OBJDIR)/%.o,$(ALL_SRCS)))

# ----- Phony targets -----
.PHONY: all clean fclean re version info help \
		cov-build cov-clean \
		headers-add headers-check init \
		malloc clean-malloc

# ----- Default -----
all: $(NAME)

$(NAME): $(ALL_OBJS)
	$(AR) rcs $@ $^

# ----- Coverage -----
cov-build: cov-clean
	@$(MAKE) HAJ_COV=1 all

cov-clean:
	@rm -f $(OBJDIR)/*.gcda $(OBJDIR)/*.gcno

# ----- Informational targets -----
version:
	@echo "$(HAJ_VERSION)"

info:
	@echo "hajlib version  : $(HAJ_VERSION)"
	@echo "Target OS       : $(TARGET_OS)"
	@echo "Target arch     : $(TARGET_ARCH)"
	@echo "Compiler        : $(CC)"
	@echo "Archiver        : $(AR)"
	@echo "Cross-compiling : $(CROSS_COMPILING)"
	@echo "Coverage        : $(HAJ_COV)"
	@echo "Library         : $(HAJ_LIB)"
	@echo "Linker script   : $(HAJ_LDSCRIPT)"
	@echo "Sources         : $(words $(ALL_SRCS)) files"
	@echo "Objects         : $(words $(ALL_OBJS)) files"

help:
	@echo "hajlibc targets:"
	@echo "  make                 build $(NAME)"
	@echo "  make clean           remove object files"
	@echo "  make fclean          remove object files and the library"
	@echo "  make re              fclean + all"
	@echo "  make version         print the current version"
	@echo "  make info            print build configuration"
	@echo "  make cov-build       build with coverage"
	@echo "  make cov-clean       remove coverage files"
	@echo "  make headers-add     add/update SPDX headers in source files"
	@echo "  make headers-check   verify SPDX headers"
	@echo "  make init            install git hooks"
	@echo "  make help            this message"
	@echo ""
	@echo "Cross-compilation (see mk/cross.mk):"
	@echo "  make WIN64=1         compile for 64-bit Windows (mingw)"
	@echo "  make WIN32=1         compile for 32-bit Windows (mingw)"
	@echo "  make CROSS_WIN=1     alias for WIN64"
	@echo "  make NATIVE=1        force native compilation"
	@echo ""
	@echo "Build switches:"
	@echo "  make SHOW_FLAGS=1    print every compilation command"
	@echo "  make UNWIND=1        one progress line per file"
	@echo "  make HAJ_COV=1       coverage build"
	@echo "  make HAJ_PTHREAD_PROCESS_SHARED=1   process-shared mutexes"

# ----- Housekeeping -----
clean:
	@rm -rf $(OBJDIR)

fclean: clean
	@rm -f $(NAME)

re: fclean all

init:
	@echo "Installing git hooks..."
	@git config core.hooksPath .githooks
	@chmod +x .githooks/pre-commit
	@chmod +x scripts/*.sh
	@echo "Done. Git hooks are now active."

headers-add:
	@for f in $$(find src include -type f \( -name '*.c' -o -name '*.h' -o -name '*.S' \)); do \
		./scripts/header.sh "$$f"; \
	done

headers-check:
	@./scripts/header-check.sh

# ----- Malloc preload library -----
# Built as a standalone shared object loaded via LD_PRELOAD. Kept out of
# the main archive and out of the default `all` target.
#
# Not cross-compiled: LD_PRELOAD is a POSIX/Linux concept, and mingw has
# no equivalent.

HOSTTYPE	?= $(shell uname -m)_$(shell uname -s)
MALLOC_INC	:= -Iinclude/malloc. -I$(MALLOC_DIR)/include

MALLOC_CFLAGS := \
	-ffreestanding \
	-fno-builtin \
	-fno-stack-protector \
	-fno-asynchronous-unwind-tables \
	-fno-unwind-tables \
	-DHMALLOC_SHARED_LIB -DHMALLOC_SHARED_LIB_EXPORT \
	-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 \
	-fPIC \
	-fvisibility=hidden \
	-fno-omit-frame-pointer \
	-Wall -Wextra -Werror -Wpedantic \
	-O3

MALLOC_LDFLAGS := \
	-shared \
	-pthread \
	-Wl,--build-id=none

MALLOC_TARGET	:= libft_malloc_$(HOSTTYPE).so
MALLOC_LINK		:= libft_malloc.so

$(MALLOC_TARGET): $(MALLOC_SRCS) $(DEBUGINFO_SRCS)
	$(CC) $(MALLOC_CFLAGS) $(MALLOC_INC) -o $@ $^ $(MALLOC_LDFLAGS)

$(MALLOC_LINK): $(MALLOC_TARGET)
	ln -sf $< $@

malloc: $(MALLOC_LINK)

clean-malloc:
	@rm -f $(MALLOC_TARGET) $(MALLOC_LINK)
