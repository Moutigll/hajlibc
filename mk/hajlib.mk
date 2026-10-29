# hajlib.mk - public build interface for programs linked against hajlibc.
#
# Include this file from a user's Makefile:
#
#   HAJ_ROOT := path/to/hajlibc
#   include $(HAJ_ROOT)/mk/hajlib.mk
#
#   my_prog: main.c
#       $(CC) $(HAJ_CPPFLAGS) $(HAJ_CFLAGS) $< \
#             $(HAJ_LIBS) $(HAJ_LDFLAGS) -o $@
#
# This file is self-contained. It is the single source of truth for
# the flags required to compile and link against hajlibc.
#
# Defines:
#   HAJ_ROOT         root of the hajlibc source tree
#   HAJ_INCLUDE      path to include/
#   HAJ_LIB          path to libhajc.a
#   HAJ_LDSCRIPT     path to the linker script
#   HAJ_COMMON_CFLAGS  flags required by *any* TU compiled against hajlibc
#   HAJ_CPPFLAGS     preprocessor flags for users
#   HAJ_CFLAGS       C compiler flags for users
#   HAJ_LDFLAGS      linker flags for users
#   HAJ_LIBS         libraries for users
#
# All variables can be overridden on the command line:
#   make HAJ_ROOT=/usr/local

# Root of the hajlibc source tree.
# If not set, guess it from the location of this file (mk/ → parent).
HAJ_ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))..)

# Public paths.
HAJ_INCLUDE		:= $(HAJ_ROOT)/include
HAJ_LIB			:= $(HAJ_ROOT)/libhajc.a
HAJ_LDSCRIPT	:= $(HAJ_ROOT)/mk/hajlibc.ld

# Flags required to compile against hajlibc.
# hajlibc replaces the libc: disable every libc-provided feature the
# compiler would otherwise assume (builtins, stack protector, PIE, etc.).
HAJ_COMMON_CFLAGS := \
	-std=c11 \
	-ffreestanding \
	-fno-builtin \
	-fno-stack-protector \
	-fno-asynchronous-unwind-tables \
	-fno-unwind-tables \
	-fno-pie \
	-fno-pic \
	-U_FORTIFY_SOURCE \
	-D_FORTIFY_SOURCE=0 \
	-fno-omit-frame-pointer

# Optional feature toggle (must match how the lib was built).
ifeq ($(HAJ_PTHREAD_PROCESS_SHARED),1)
  HAJ_COMMON_CFLAGS += -DHAJ_PTHREAD_PROCESS_SHARED
endif

HAJ_CPPFLAGS	:= -I$(HAJ_INCLUDE)
HAJ_CFLAGS		:= $(HAJ_COMMON_CFLAGS)
HAJ_LDFLAGS		:= \
	-nostartfiles \
	-nodefaultlibs \
	-static \
	-Wl,--build-id=none \
	-T $(HAJ_LDSCRIPT)
HAJ_LIBS		:= $(HAJ_LIB)
