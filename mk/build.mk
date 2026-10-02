# build.mk - common build rules for hajlib.
#
# This file knows how to compile .c and .S files into .o files, and
# how to archive .o files into .a files. It is included by the
# top-level Makefile after all configuration is done.
#
# Variables it uses (must be defined before including):
#   CC, AR, CFLAGS, CPPFLAGS, OBJDIR
#
# Variables it defines:
#   OBJS  list of all object files to build

# Object directory
OBJDIR ?= objs

# Create object dir on demand
$(OBJDIR):
	@mkdir -p $(OBJDIR)

# Compile .c to .o
$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COV_CFLAGS) -c $< -o $@

# Compile .S (preprocessed assembly) to .o
$(OBJDIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# The file atomic_aarch64.S can use LSE instructions, which are only available on armv8.1 and later.
# To compile on older armv8.0 hardware, we need to specify the -march=armv8-a+lse flag when compiling this file.
# Using or not the instruction set is determined at runtime by the CPU feature detection code.
objs/src/thread/internal/atomics_aarch64.o: src/thread/internal/atomics_aarch64.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -march=armv8-a+lse -c $< -o $@
