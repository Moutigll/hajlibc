# build.mk - common build rules for hajlib.
#
# This file knows how to compile .c and .S files into .o files, and
# how to archive .o files into .a files. It is included by the
# top-level Makefile after all configuration is done.
#
# Variables it uses (must be defined before including):
#   CC, AR, CFLAGS, CPPFLAGS, OBJDIR, OBJS
#
# Variables it defines:
#   TOTAL_OBJS, PROGRESS

# Object directory
OBJDIR ?= objs

# Create object dir on demand
$(OBJDIR):
	@mkdir -p $(OBJDIR)

PROGRESS	:= $(OBJDIR)/.progress
TOTAL_OBJS	= $(words $(ALL_SRCS))

ifeq ($(MAKELEVEL),0)
$(shell rm -f $(PROGRESS) 2>/dev/null)
endif

# SHOW_FLAGS	: Print the full command line before executing it
# UNWIND		: Disable carriage return line overwrite (force non-TTY mode)
define _haj_progress
	@( \
		flock 9; \
		n=$$(($$(cat $(PROGRESS) 2>/dev/null || echo 0) + 1)); \
		echo $$n > $(PROGRESS); \
		total=$(TOTAL_OBJS); \
		msg="$(1)"; \
		[ "$(SHOW_FLAGS)" = "1" ] && printf '%s\n' "$$msg" >&2; \
		if [ "$(UNWIND)" != "1" ] && [ -t 1 ]; then \
			if [ $$n -lt $$total ]; then \
				printf "\r[%0$${#total}d/%d] %s\033[K" $$n $$total "$$msg"; \
			else \
				printf "\r[%0$${#total}d/%d] %s\033[K\n" $$n $$total "$$msg"; \
			fi; \
		else \
			printf "[%0$${#total}d/%d] %s\n" $$n $$total "$$msg"; \
		fi; \
	) 9>>$(PROGRESS)
endef

define _haj_compile
	$(call _haj_progress,$(1))
	@$(if $(filter 1,$(SHOW_FLAGS)),echo '  $(2)' && )$(2)
endef

# Compile .c to .o
$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(call _haj_compile,CC $<,$(CC) $(CPPFLAGS) $(CFLAGS) $(COV_CFLAGS) -c $< -o $@)

# Compile .S (preprocessed assembly) to .o
$(OBJDIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(call _haj_compile,AS $<,$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@)

# The file atomic_aarch64.S can use LSE instructions, which are only available on armv8.1 and later.
# To compile on older armv8.0 hardware, we need to specify the -march=armv8-a+lse flag when compiling this file.
# Using or not the instruction set is determined at runtime by the CPU feature detection code.
objs/src/thread/internal/atomics_aarch64.o: src/thread/internal/atomics_aarch64.S
	@mkdir -p $(dir $@)
	$(call _haj_compile,AS $< [LSE],$(CC) $(CFLAGS) -march=armv8-a+lse -c $< -o $@)
