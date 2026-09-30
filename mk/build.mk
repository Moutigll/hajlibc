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
