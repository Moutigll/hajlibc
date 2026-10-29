# config.mk - internal build configuration for hajlibc.
#
# Reserved for building hajlibc itself. Users linking against hajlibc
# must include mk/hajlib.mk only, NOT this file.
#
# This file:
#   - includes cross.mk (toolchain)
#   - includes hajlib.mk (public flag set, single source of truth)
#   - derives the internal flags CPPFLAGS / CFLAGS / LDFLAGS / LDLIBS
#
# Defines:
#   CPPFLAGS, CFLAGS, LDFLAGS, LDLIBS    internal build flags
#   COV_CFLAGS, COV_LDFLAGS, COV_LDLIBS  coverage flags (if HAJ_COV=1)
#
# Ordering contract: config.mk must be included AFTER cross.mk if
# cross.mk is included separately; but including config.mk alone is
# sufficient because it includes cross.mk itself.

_HAJ_CONFIG_MK_DIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))
include $(_HAJ_CONFIG_MK_DIR)/cross.mk
include $(_HAJ_CONFIG_MK_DIR)/hajlib.mk

# --- Coverage -------------------------------------------------------------
HAJ_COV ?= 0
ifeq ($(HAJ_COV),1)
  COV_CFLAGS	:= --coverage -fprofile-arcs -ftest-coverage -O0 -g
  COV_LDFLAGS	:= --coverage
  COV_LDLIBS	:= $(shell $(CC) -print-file-name=libclang_rt.profile-$(shell uname -m).a)
else
  COV_CFLAGS	:=
  COV_LDFLAGS	:=
  COV_LDLIBS	:=
endif

# --- Internal flags -------------------------------------------------------
# Start from the public interface (single source of truth), then add
# cross-compilation extras and lib-specific warnings/optimisation.
CPPFLAGS	:= $(HAJ_CPPFLAGS)	$(CROSS_CPPFLAGS)
CFLAGS		:= $(HAJ_CFLAGS)	$(CROSS_CFLAGS) -Wall -Wextra -Wpedantic -O3
LDFLAGS		:= $(HAJ_LDFLAGS)	$(CROSS_LDFLAGS)
LDLIBS		:= $(HAJ_LIBS)

# Extra flags for sub-projects (e.g. the test harness) that need to
# rebuild the library with different compile-time switches without
# duplicating the whole flag set. These are appended AFTER the lib's
# own flags, so they cannot break the lib's invariants.
HAJ_EXTRA_CPPFLAGS	?=
HAJ_EXTRA_CFLAGS	?=
CPPFLAGS			+= $(HAJ_EXTRA_CPPFLAGS)
CFLAGS				+= $(HAJ_EXTRA_CFLAGS)

# In cross builds, -march=native/-mtune=native are meaningless on the
# target and -Werror frequently trips on foreign headers. Strip them.
ifeq ($(CROSS_COMPILING),1)
  CFLAGS := $(filter-out -march=native -mtune=native -Werror,$(CFLAGS))
endif

# -Werror belongs to native lib builds only (not appended above so that
# cross builds above stay clean).
ifeq ($(CROSS_COMPILING),0)
  CFLAGS += -Werror
endif

# --- Export for sub-makefiles --------------------------------------------
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
export COV_CFLAGS
export COV_LDFLAGS
export COV_LDLIBS
export TARGET_OS
export TARGET_ARCH
export TARGET_EXT
export CROSS_COMPILING
