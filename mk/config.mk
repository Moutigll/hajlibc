# config.mk - toolchain and flags configuration for hajlib.
#
# This file defines all the flags needed to build and link
# against hajlib. It can be included from a user's Makefile to
# get the right flags without having to remember them.
#
# Usage (from a user Makefile):
#
#   HAJ_ROOT := path/to/hajlib
#   include $(HAJ_ROOT)/config.mk
#
#   my_prog: main.c
#       $(CC) $(HAJ_CPPFLAGS) $(HAJ_CFLAGS) $< \
#             $(HAJ_LIBS) $(HAJ_LDFLAGS) -o $@
#
# Variables defined:
#
#   Build (project):
#     CC, AR, LD            compiler, archiver, linker
#     CPPFLAGS              preprocessor flags (include paths)
#     CFLAGS                C compiler flags
#     LDFLAGS               linker flags
#     LDLIBS                libraries to link
#
#   Public (user):
#     HAJ_ROOT              root of the hajlib source tree
#     HAJ_INCLUDE           path to include/
#     HAJ_LIB               path to libhajc.a
#     HAJ_CPPFLAGS          preprocessor flags for users
#     HAJ_CFLAGS            C compiler flags for users
#     HAJ_LDFLAGS           linker flags for users
#     HAJ_LIBS              libraries for users
#
# All variables can be overridden on the command line, e.g.:
#   make CC=gcc
#   make HAJ_ROOT=/usr/local

# Root of the hajlib source tree
# If HAJ_ROOT is not set, guess it from the location of this file.
HAJ_ROOT ?= $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

# Toolchain
CC		?= clang
AR		?= ar
LD		?= $(CC)

# Public paths (for users of hajlib)
HAJ_INCLUDE	:= $(HAJ_ROOT)/include
HAJ_LIB		:= $(HAJ_ROOT)/libhajc.a

# Common compiler flags:
# These flags are shared between building hajlib and building
# programs against hajlib. They are needed because hajlib
# replaces the libc: no builtins, no libc headers, no stack
# protector from the libc, etc.

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
	-D_FORTIFY_SOURCE=0

# Public flags (for users of hajlib)
HAJ_CPPFLAGS	:= -I$(HAJ_INCLUDE)
HAJ_CFLAGS	:= $(HAJ_COMMON_CFLAGS)
HAJ_LDFLAGS	:= \
	-nostartfiles \
	-nodefaultlibs \
	-static \
	-Wl,--build-id=none
HAJ_LIBS	:= $(HAJ_LIB)

# Project flags (for building hajlib itself)
# We add warning flags and optimization on top of the common
# flags. These are only used by the hajlib build, not by users.
CPPFLAGS	:= -I$(HAJ_INCLUDE)
CFLAGS		:= \
	$(HAJ_COMMON_CFLAGS) \
	-Wall \
	-Wextra \
	-Werror \
	-Wpedantic \
	-O2
LDFLAGS		:= $(HAJ_LDFLAGS)
LDLIBS		:= $(HAJ_LIBS)

# Export for sub-makefiles
export CC
export AR
export LD
export CPPFLAGS
export CFLAGS
export LDFLAGS
export LDLIBS
export HAJ_ROOT
export HAJ_INCLUDE
export HAJ_LIB
export HAJ_CPPFLAGS
export HAJ_CFLAGS
export HAJ_LDFLAGS
export HAJ_LIBS
