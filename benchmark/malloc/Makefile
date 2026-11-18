# =============================================================================
# Memory allocator benchmark
# =============================================================================
# Targets:
#   make, make all          build + run every allocator + generate report.html
#   make build              build the benchmark binaries only
#   make check-deps         show which allocators are available
#   make list               show how each allocator is linked / loaded
#   make run                run every available benchmark
#   make run-hajlibc        run hajlibc only (previous run is archived)
#   make run-hajlibc-static run hajlibc_static only (same archive rotation)
#   make report             regenerate report.html from results/
#   make report-hajlibc     run-hajlibc + report
#   make report-hajlibc-static  run-hajlibc-static + report
#   make clean              remove bin/*, results/*, report.html, scores.json
#   make fclean             clean + remove bin/ and results/
#   make clean-old          remove the archived hajlibc run (*_hajlibc_old.csv)
#   make re                 fclean + all
#
# bench.c is built in one of two modes:
#   host          glibc, hajlibc, mimalloc, tcmalloc, jemalloc. Plain
#                 malloc/free/calloc/realloc against the system headers; the
#                 allocator under test is selected by preloading it
#                 (LD_PRELOAD / DYLD_INSERT_LIBRARIES) or by linking it.
#   freestanding  hajlibc_static (-DBENCH_STATIC). Only hajlibc headers,
#                 calls hmalloc/hfree/..., links libhajc.a statically without
#                 any system libc.
# =============================================================================

CC		?= clang
PY		?= python3

SRC		:= bench.c
BINDIR	:= bin
RESDIR	:= results

HAJ_ROOT	:= ../..
include $(HAJ_ROOT)/mk/config.mk

HAJ_SO		:= $(HAJ_ROOT)/libft_malloc.so

# ----- Platform -----
ifeq ($(TARGET_OS),Darwin)
  PRELOAD	:= DYLD_FORCE_FLAT_NAMESPACE=1 DYLD_INSERT_LIBRARIES
  SO_EXT	:= .dylib
else
  PRELOAD	:= LD_PRELOAD
  SO_EXT	:= .so
endif

# The freestanding bench (ELF linker script, -static, no libc) only works on
# ELF targets: Linux and FreeBSD (x86_64, aarch64).
HAJ_STATIC_OK := $(if $(filter Linux FreeBSD,$(TARGET_OS)),1,0)

# External allocators are looked up in the usual library directories:
# Debian/Ubuntu multiarch, Fedora lib64, BSD /usr/local, macOS Homebrew.
MULTIARCH	:= $(shell $(CC) -dumpmachine 2>/dev/null)
LIB_DIRS	:= /usr/lib/$(MULTIARCH) /usr/lib64 /usr/lib \
			   /usr/local/lib /opt/homebrew/lib
find_lib	= $(firstword $(wildcard $(foreach d,$(LIB_DIRS),$(d)/$(1)$(SO_EXT) $(d)/$(1)$(SO_EXT).*)))

MIMALLOC_SO	:= $(call find_lib,libmimalloc)
TCMALLOC_SO	:= $(call find_lib,libtcmalloc_minimal)
JEMALLOC_SO	:= $(call find_lib,libjemalloc)

# ----- Compiler flags -----
BENCH_HOST_CFLAGS	:= -Wall -Wextra -g -D_FORTIFY_SOURCE=0 -pthread
BENCH_HOST_LDFLAGS	:= -pthread

BENCH_STATIC_CPPFLAGS	:= $(HAJ_CPPFLAGS) -DBENCH_STATIC
BENCH_STATIC_CFLAGS		:= $(HAJ_CFLAGS)
BENCH_STATIC_LDFLAGS	:= $(HAJ_LDFLAGS)
BENCH_STATIC_LIBS		:= $(HAJ_LIB)

# -----------------------------------------------------------------------------
# Allocators
#
#   LIB_<a>      file the allocator needs; empty = not available
#                (glibc is always available and needs none)
#   MODE_<a>     how it is selected, shown by `check-deps` and `list`
#   LINK_<a>     extra link flags (host mode)
#   PRELOAD_<a>  library preloaded at run time
#   MISSING_<a>  message shown when it is not available
# -----------------------------------------------------------------------------
# Remove or add allocators to the list below to change which ones are built and run.
ALLOCS		:= glibc hajlibc hajlibc_static mimalloc tcmalloc jemalloc
HOST_ALLOCS	:= $(filter-out hajlibc_static,$(ALLOCS))

LIB_hajlibc			:= $(HAJ_SO)
LIB_hajlibc_static	:= $(if $(filter 1,$(HAJ_STATIC_OK)),$(HAJ_LIB))
LIB_mimalloc		:= $(MIMALLOC_SO)
LIB_tcmalloc		:= $(TCMALLOC_SO)
LIB_jemalloc		:= $(JEMALLOC_SO)

MODE_glibc			:= default
MODE_hajlibc		:= $(lastword $(PRELOAD))
MODE_hajlibc_static	:= freestanding static link
MODE_mimalloc		:= linked
MODE_tcmalloc		:= $(lastword $(PRELOAD))
MODE_jemalloc		:= $(lastword $(PRELOAD))

LINK_mimalloc		:= $(MIMALLOC_SO) -Wl,-rpath,$(dir $(MIMALLOC_SO))

PRELOAD_hajlibc		:= $(HAJ_SO)
PRELOAD_tcmalloc	:= $(TCMALLOC_SO)
PRELOAD_jemalloc	:= $(JEMALLOC_SO)

MISSING_hajlibc			:= NOT BUILT (run 'make' at repo root)
MISSING_hajlibc_static	:= $(if $(filter 1,$(HAJ_STATIC_OK)),NOT BUILT (run 'make' at repo root),unsupported on $(TARGET_OS) (ELF only))
MISSING_mimalloc		:= NOT FOUND (libmimalloc$(SO_EXT) in $(LIB_DIRS))
MISSING_tcmalloc		:= NOT FOUND (libtcmalloc_minimal$(SO_EXT) in $(LIB_DIRS))
MISSING_jemalloc		:= NOT FOUND (libjemalloc$(SO_EXT) in $(LIB_DIRS))

# $(call is_available,a) -> non-empty if allocator `a` can be built and run
is_available = $(or $(filter glibc,$(1)),$(wildcard $(LIB_$(1))))

# $(call run_cmd,a) -> shell command running the benchmark of allocator `a`
run_cmd = $(if $(PRELOAD_$(1)),$(PRELOAD)="$(PRELOAD_$(1))" )./$(BINDIR)/bench_$(1) $(1) $(RESDIR)

AVAILABLE := $(foreach a,$(ALLOCS),$(if $(call is_available,$(a)),$(a)))
BUILDABLE := $(addprefix $(BINDIR)/bench_,$(AVAILABLE))

# ----- Main targets -----
all: build run report

build: check-deps $(BUILDABLE)

check-deps:
	@echo "--- Available allocators ---"
	@$(foreach a,$(ALLOCS), \
		printf "  %-15s: %s\n" "$(a)" \
			"$(if $(call is_available,$(a)),found ($(or $(LIB_$(a)),system libc)) [$(MODE_$(a))],$(MISSING_$(a)))";)
	@echo "----------------------------"

list:
	@echo "Configured allocators:"
	@$(foreach a,$(ALLOCS), \
		printf "  - %-16s ->  %s\n" "$(a)" "$(or $(LINK_$(a)),($(MODE_$(a))))";)

# ----- Build -----
$(BINDIR) $(RESDIR):
	@mkdir -p $@

# Host mode: one rule per allocator, differing only by LINK_<a>.
define HOST_RULE
$(BINDIR)/bench_$(1): $(SRC) | $(BINDIR)
	@printf "  CC   %s\n" "$$(notdir $$@)"
	@$$(CC) $$(BENCH_HOST_CFLAGS) -o $$@ $$(SRC) \
		$$(BENCH_HOST_LDFLAGS) $$(LINK_$(1))
endef
$(foreach a,$(HOST_ALLOCS),$(eval $(call HOST_RULE,$(a))))

# Freestanding mode: -DBENCH_STATIC, static link against libhajc.a.
$(BINDIR)/bench_hajlibc_static: $(SRC) $(HAJ_LIB) | $(BINDIR)
	@printf "  CC   %s (freestanding)\n" "$(notdir $@)"
	@$(CC) $(BENCH_STATIC_CPPFLAGS) $(BENCH_STATIC_CFLAGS) \
		-o $@ $(SRC) \
		$(BENCH_STATIC_LDFLAGS) $(BENCH_STATIC_LIBS) \
		-pthread

# ----- Run -----
# Every hajlibc run first moves the previous results to *_hajlibc_old.csv,
# so the last two runs can be compared.
backup-hajlibc: | $(RESDIR)
	@for f in results_hajlibc.csv timeseries_hajlibc.csv; do \
		if [ -f "$(RESDIR)/$$f" ]; then \
			old="$${f%.csv}_old.csv"; \
			mv -f "$(RESDIR)/$$f" "$(RESDIR)/$$old"; \
			echo "  ↻ hajlibc: previous run archived -> $$old"; \
		fi; \
	done

# A crashing benchmark does not stop the others: the rows it already wrote
# are kept and reported as such.
run: backup-hajlibc $(BUILDABLE) | $(RESDIR)
	@$(foreach a,$(AVAILABLE), \
		printf "\n>>> Launching benchmark: %s\n" "$(a)"; \
		$(call run_cmd,$(a)) || { \
			rc=$$?; csv="$(RESDIR)/results_$(a).csv"; \
			lines=$$(wc -l < "$$csv" 2>/dev/null || echo 0); \
			echo " $(a) crashed (exit $$rc), $$lines line(s) kept in $$csv"; \
		};)
	@printf "\nResults are in: %s/\n" "$(RESDIR)"

run-hajlibc: backup-hajlibc $(BINDIR)/bench_hajlibc | $(RESDIR)
	@printf "\n>>> Launching benchmark: hajlibc (only, %s)\n" "$(MODE_hajlibc)"
	@$(call run_cmd,hajlibc)
	@printf "\nhajlibc results refreshed in: %s/\n" "$(RESDIR)"

run-hajlibc-static: backup-hajlibc $(BINDIR)/bench_hajlibc_static | $(RESDIR)
	@printf "\n>>> Launching benchmark: hajlibc_static (only, %s)\n" "$(MODE_hajlibc_static)"
	@$(call run_cmd,hajlibc_static)
	@printf "\nhajlibc_static results refreshed in: %s/\n" "$(RESDIR)"

# ----- Report -----
report:
	@printf "  PY   %s\n" "analyze.py --output report.html"
	@$(PY) analyze.py --results-dir $(RESDIR) --output report.html
	@echo "Report generated: report.html"

report-hajlibc: run-hajlibc report
report-hajlibc-static: run-hajlibc-static report

# ----- Cleanup -----
clean:
	@printf "  CLEAN %s\n" "$(BINDIR)/* $(RESDIR)/* report.html scores.json"
	@rm -f $(BINDIR)/* $(RESDIR)/* report.html scores.json

fclean: clean
	@printf "  FCLEAN %s\n" "$(BINDIR)/ $(RESDIR)/"
	@rm -rf $(BINDIR) $(RESDIR)

clean-old:
	@rm -f $(RESDIR)/results_hajlibc_old.csv $(RESDIR)/timeseries_hajlibc_old.csv
	@echo "Archived hajlibc results removed."

re: fclean
	@$(MAKE) --no-print-directory all

.PHONY:	all build check-deps list backup-hajlibc \
		run run-hajlibc run-hajlibc-static \
		report report-hajlibc report-hajlibc-static \
		clean fclean clean-old re
