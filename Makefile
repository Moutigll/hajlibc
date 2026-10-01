# hajlib - Makefile
#
# A POSIX-like C library, no libc dependency, multi-OS multi-arch.
#
# Targets:
#   all       build libhajc.a (default)
#   clean     remove object files
#   fclean    remove object files and the library
#   re        fclean + all
#   version   print the current version
#   info      print build configuration
#   help      this message
#
# Cross-compilation:
#   make WIN64=1         compile for 64-bit Windows (mingw)
#   make WIN32=1         compile for 32-bit Windows (mingw)
#   make NATIVE=1        force native compilation
#

# Configuration (toolchain, flags, public paths)
HAJ_ROOT := $(CURDIR)
include mk/config.mk
# Cross-compilation (must come before source selection)
include mk/cross.mk
# Version (extracted from include/haj/version.h)
include mk/version.mk

# Sources (portable + OS/arch-specific)
include mk/sources.mk
include mk/targets.mk

# Build rules
include mk/build.mk

# Library name-
NAME		:= libhajc.a
OBJDIR		:= objs

# Collect all source files
# The order matters: low-level modules first, high-level after.
# The linker scans the archive from left to right and picks the
# objects that resolve unresolved symbols.
ALL_SRCS := \
	$(CPU_SRCS) \
	$(CRT_SRCS) \
	$(CRT_START_SRCS) \
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
	$(STDLIB_SRCS) \
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

# Info targets
.PHONY: version info help clean fclean re headers-add headers-check init cov-build cov-clean

init:
	@echo "Installing git hooks..."
	@git config core.hooksPath .githooks
	@chmod +x .githooks/pre-commit
	@chmod +x scripts/*.sh
	@echo "Done. Git hooks are now active."

# Default target
all: $(NAME)

$(NAME): $(ALL_OBJS)
	$(AR) rcs $@ $^

cov-build: cov-clean
	@$(MAKE) HAJ_COV=1 all

cov-clean:
	@rm -f $(OBJDIR)/*.gcda $(OBJDIR)/*.gcno

version:
	@echo "$(HAJ_VERSION)"

headers-add:
	@for f in $$(find src include -type f \( -name '*.c' -o -name '*.h' -o -name '*.S' \)); do \
		./scripts/header.sh "$$f"; \
	done

headers-check:
	@./scripts/header-check.sh

info:
	@echo "hajlib version  : $(HAJ_VERSION)"
	@echo "Target OS       : $(TARGET_OS)"
	@echo "Target arch     : $(TARGET_ARCH)"
	@echo "Compiler        : $(CC)"
	@echo "Archiver        : $(AR)"
	@echo "Cross-compiling : $(CROSS_COMPILING)"
	@echo "Sources         : $(words $(ALL_SRCS)) files"
	@echo "Objects         : $(words $(ALL_OBJS)) files"

help:
	@echo "hajlib targets:"
	@echo "  make          build libhajc.a"
	@echo "  make clean    remove object files"
	@echo "  make fclean   remove object files and the library"
	@echo "  make headers-add    add/update headers in source files"
	@echo "  make headers-check  check headers in source files"
	@echo "  make re       fclean + all"
	@echo "  make version  print the current version"
	@echo "  make cov-build  build with coverage"
	@echo "  make cov-clean  clean coverage files"
	@echo "  make info     print build configuration"
	@echo "  make init     install git hooks"
	@echo "  make help     this message"
	@echo ""
	@echo "Cross-compilation:"
	@echo "  make WIN64=1  compile for 64-bit Windows (mingw)"
	@echo "  make WIN32=1  compile for 32-bit Windows (mingw)"
	@echo "  make NATIVE=1 force native compilation"
	@echo ""
	@echo "See mk/cross.mk for details."

# Clean
clean:
	@rm -rf $(OBJDIR)

fclean: clean
	@rm -f $(NAME)

re: fclean all
