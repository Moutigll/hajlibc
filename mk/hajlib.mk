# hajlib.mk - common flags for building programs against hajlib.
#
# Include this file in your Makefile to get the correct flags:
#
#   include path/to/hajlib.mk
#
# It defines:
#   HAJ_CFLAGS    C compiler flags for hajlib
#   HAJ_CPPFLAGS  C preprocessor flags (include paths)
#   HAJ_LDFLAGS   Linker flags
#   HAJ_LIBS      Libraries to link
#
# You can use them like this:
#
#   CFLAGS += $(HAJ_CFLAGS)
#   CPPFLAGS += $(HAJ_CPPFLAGS)
#   LDFLAGS += $(HAJ_LDFLAGS)
#   LDLIBS += $(HAJ_LIBS)

# Root of the hajlib installation (adjust if needed).
HAJ_ROOT ?= $(dir $(lastword $(MAKEFILE_LIST)))

# Include path.
HAJ_CPPFLAGS := -I$(HAJ_ROOT)/include

# C compiler flags.
HAJ_CFLAGS := \
	-std=c11 \
	-ffreestanding \
	-fno-builtin \
	-fno-stack-protector \
	-fno-asynchronous-unwind-tables \
	-fno-unwind-tables \
	-fno-pie \
	-fno-pic

# Linker flags.
HAJ_LDFLAGS := \
	-nostartfiles \
	-nodefaultlibs \
	-static \
	-Wl,--build-id=none

# Libraries.
HAJ_LIBS := $(HAJ_ROOT)/libhajc.a
