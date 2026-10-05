# cross.mk - add cross-compilation support for Makefile.
#
# Usage:
#   include cross.mk
#
# Defines:
#   CC, AR, CFLAGS, LDFLAGS, TARGET_EXT,
#   CROSS_COMPILING, TARGET_OS, TARGET_ARCH
#
# Targets:
#   make win32    build for Windows 32-bit (mingw)
#   make win64    build for Windows 64-bit (mingw)
#   make cross    alias for win64
#   make native   force native compilation
#
# Variables:
#   WIN32=1        force 32-bit Windows compilation
#   WIN64=1        force 64-bit Windows compilation
#   CROSS_WIN=1    alias for WIN64
#   NATIVE=1       force native compilation
#   CC=<compiler>  override compiler

.DEFAULT_GOAL := all

# --- Detect host OS ---
OS := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(OS),Windows_NT)
  DETECTED_OS := Windows
else ifeq ($(OS),Linux)
  DETECTED_OS := Linux
else ifeq ($(OS),Darwin)
  DETECTED_OS := Darwin
else ifeq ($(findstring MINGW,$(OS)),MINGW)
  DETECTED_OS := Windows
else ifeq ($(findstring CYGWIN,$(OS)),CYGWIN)
  DETECTED_OS := Windows
endif

# --- Default native compilation ---
ifeq ($(NATIVE),1)
  CC ?= clang
  AR := ar
  TARGET_EXT :=
  CROSS_COMPILING := 0
else ifneq ($(WIN32),1)
  ifneq ($(WIN64),1)
    ifneq ($(CROSS_WIN),1)
      CC ?= clang
      AR := ar
      TARGET_EXT :=
      CROSS_COMPILING := 0
    endif
  endif
endif

# --- Windows 64-bit ---
ifeq ($(WIN64),1)
  CC := x86_64-w64-mingw32-gcc
  AR := x86_64-w64-mingw32-ar
  TARGET_EXT := .exe
  WINDRES := x86_64-w64-mingw32-windres
  CFLAGS += -DWIN32 -D_WIN32 -DWIN64
  LDFLAGS += -lws2_32 -lwinmm -static-libgcc -static
  CROSS_COMPILING := 1
  TARGET_OS := Windows
  TARGET_ARCH := x86_64
endif

# --- Windows 32-bit ---
ifeq ($(WIN32),1)
  CC := i686-w64-mingw32-gcc
  AR := i686-w64-mingw32-ar
  TARGET_EXT := .exe
  WINDRES := i686-w64-mingw32-windres
  CFLAGS += -DWIN32 -D_WIN32
  LDFLAGS += -lws2_32 -lwinmm -static-libgcc -static
  CROSS_COMPILING := 1
  TARGET_OS := Windows
  TARGET_ARCH := i686
endif

# --- Alias CROSS_WIN for WIN64 ---
ifeq ($(CROSS_WIN),1)
  WIN64 := 1
  CC := x86_64-w64-mingw32-gcc
  AR := x86_64-w64-mingw32-ar
  TARGET_EXT := .exe
  WINDRES := x86_64-w64-mingw32-windres
  CFLAGS += -DWIN32 -D_WIN32 -DWIN64
  LDFLAGS += -lws2_32 -lwinmm -static-libgcc -static
  CROSS_COMPILING := 1
  TARGET_OS := Windows
  TARGET_ARCH := x86_64
endif

# --- Cross vs native settings ---
ifeq ($(CROSS_COMPILING),1)
  CROSS_PREFIX := $(CC:%-gcc=%)
  # Remove flags incompatible with cross-compilation.
  BASE_FLAGS := $(filter-out -march=native,$(BASE_FLAGS))
  BASE_FLAGS := $(filter-out -mtune=native,$(BASE_FLAGS))
  BASE_FLAGS := $(filter-out -Werror,$(BASE_FLAGS))
  # Add POSIX and GNU source definitions for cross-compilation.
  CFLAGS += -D_POSIX_C_SOURCE=200809L
  CFLAGS += -D_GNU_SOURCE
else
  CROSS_PREFIX :=
  TARGET_OS := $(DETECTED_OS)
  TARGET_ARCH := $(shell uname -m 2>/dev/null || echo unknown)
endif

# --- Check for cross-compilation tools ---
ifeq ($(CROSS_COMPILING),1)
  ifeq ($(shell command -v $(CC) 2>/dev/null),)
    $(warning $(CC) not found! Install mingw-w64:)
    $(warning   Debian/Ubuntu: sudo apt install mingw-w64)
    $(warning   Arch Linux:   sudo pacman -S mingw-w64-gcc)
    $(warning   Fedora:       sudo dnf install mingw64-gcc)
  endif
endif

# --- Cross-compilation targets ---
.PHONY: win32 win64 windows cross native help-cross

win32:
	@echo "Compiling for Windows 32-bit..."
	$(MAKE) all WIN32=1

win64:
	@echo "Compiling for Windows 64-bit..."
	$(MAKE) all WIN64=1

windows: win64
	@echo "Windows compilation complete."

cross: win64
	@echo "Cross-compilation complete."

native:
	@echo "Forcing native compilation..."
	$(MAKE) all NATIVE=1

help-cross:
	@echo "Cross-compilation targets:"
	@echo "  make win32    - Compile for Windows 32-bit"
	@echo "  make win64    - Compile for Windows 64-bit"
	@echo "  make windows  - Alias for win64"
	@echo "  make cross    - Alias for win64"
	@echo "  make native   - Force native compilation"
	@echo ""
	@echo "Variables:"
	@echo "  WIN32=1        - Force 32-bit Windows compilation"
	@echo "  WIN64=1        - Force 64-bit Windows compilation"
	@echo "  CROSS_WIN=1    - Alias for WIN64"
	@echo "  NATIVE=1       - Force native compilation"
	@echo "  CC=<compiler>  - Override compiler"
	@echo ""
	@echo "Current configuration:"
	@echo "  Host OS:        $(DETECTED_OS)"
	@echo "  Target OS:      $(TARGET_OS)"
	@echo "  Target ARCH:    $(TARGET_ARCH)"
	@echo "  Compiler:       $(CC)"
	@echo "  Cross-compile:  $(CROSS_COMPILING)"

# --- Export for sub-makefiles ---
export CC
export AR
export CFLAGS
export LDFLAGS
export TARGET_EXT
export CROSS_COMPILING
export TARGET_OS
export TARGET_ARCH
