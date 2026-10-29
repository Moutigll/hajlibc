# tests/<...>/Makefile - functional test harness for the hajlibc allocator.
#
# Consumes the public interface mk/hajlib.mk from the hajlibc tree.
# HAJ_ROOT is auto-detected from hajlib.mk; override with:
#   make HAJ_ROOT=/path/to/hajlibc ...
#
# Targets:
#   all                    run the full config matrix (default)
#   test-all               same, with a pass/fail summary
#   test-list              list available configs
#   test-<cfg>             build the lib + test binary for one config, run it
#   test-leak              leak-report demo (with / without -g)
#   test-preload           LD_PRELOAD smoke test
#   clean                  remove bin/
#   re                     clean + all
#
# Build switches for the config matrix (passed via HAJ_EXTRA_CFLAGS to
# the lib build so the test binary sees the same compile-time switches):
#   -DHMALLOC_DEBUG=N  -DHMALLOC_CANARY=1  -DHMALLOC_CHECK=1
#   -DHMALLOC_ENCODE_FREELIST=1  -DHMALLOC_STATS=0  -DHMALLOC_PROFILE=1

SHELL		:= /bin/zsh
.SHELLFLAGS	:= -o pipefail -c

# ----- Public interface of the library (single source of truth for all flags) -----
HAJ_ROOT ?= $(abspath ../../..)
include $(HAJ_ROOT)/mk/hajlib.mk

HAJ_JOBS ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# ----- Test-specific flags on top of the library interface -----
# HAJ_CFLAGS already contains -std=c11 -ffreestanding -fno-builtin ...
# HAJ_LDFLAGS already contains -nostartfiles -nodefaultlibs -static -T <ld>
# HAJ_LIBS   already contains $(HAJ_LIB)
#
# We only add what is specific to building the test binary itself.
TEST_CPPFLAGS	:= $(HAJ_CPPFLAGS)
TEST_CFLAGS		:= $(HAJ_CFLAGS) -g -O2 -Wall -Wextra -Werror -Wpedantic
TEST_LDFLAGS	:= $(HAJ_LDFLAGS)
TEST_LDLIBS		:= $(HAJ_LIBS)

BINDIR := bin

# ----- Configuration matrix -----
release_CFLAGS	:=
debug4_CFLAGS	:= -DHMALLOC_DEBUG=4
canary_CFLAGS	:= -DHMALLOC_CANARY=1
check_CFLAGS	:= -DHMALLOC_CHECK=1
encode_CFLAGS	:= -DHMALLOC_ENCODE_FREELIST=1
profile_CFLAGS	:= -DHMALLOC_PROFILE=1
nostats_CFLAGS	:= -DHMALLOC_STATS=0
paranoid_CFLAGS	:= \
	-DHMALLOC_DEBUG=3 \
	-DHMALLOC_CANARY=1 \
	-DHMALLOC_CHECK=1
all_on_CFLAGS	:= \
	-DHMALLOC_DEBUG=3 \
	-DHMALLOC_CANARY=1 \
	-DHMALLOC_CHECK=1 \
	-DHMALLOC_ENCODE_FREELIST=1 \
	-DHMALLOC_STATS=1 \
	-DHMALLOC_PROFILE=1

CONFIGS := release debug4 canary check encode profile nostats paranoid all_on

.PHONY: all test-all test-list test-leak clean fclean re \
		$(addprefix test-,$(CONFIGS)) \
		$(addprefix test-build-,$(CONFIGS)) \
		$(addprefix test-run-,$(CONFIGS))

.DEFAULT_GOAL := all

# ----- Default: full matrix -----
all: test-all

test-all:
	@pass=0; fail=0; nocompile=0; skipped=""; \
	for cfg in $(CONFIGS); do \
	    log=$$(mktemp); \
	    echo ""; \
	    echo "======================================================="; \
	    echo "  Config: $$cfg"; \
	    echo "======================================================="; \
	    if $(MAKE) -s test-build-$$cfg >$$log 2>&1; then \
	        if $(MAKE) -s test-run-$$cfg >>$$log 2>&1; then \
	            pass=$$((pass+1)); \
	        else \
	            fail=$$((fail+1)); \
	            skipped="$$skipped $$cfg(test)"; \
	        fi; \
	    else \
	        nocompile=$$((nocompile+1)); \
	        skipped="$$skipped $$cfg(build)"; \
	    fi; \
	    cat $$log; \
	    rm -f $$log; \
	done; \
	echo ""; \
	echo "======================================================="; \
	echo "  SUMMARY"; \
	echo "    passed:          $$pass"; \
	echo "    test failures:   $$fail"; \
	echo "    build failures:  $$nocompile"; \
	echo "    failed configs:$$skipped"; \
	echo "======================================================="; \
	[ $$nocompile -eq 0 ] && [ $$fail -eq 0 ]

test-list:
	@echo "Available configurations:"
	@for c in $(CONFIGS); do echo "  - $$c"; done

# Each config generates three targets:
#   test-build-<cfg>   build lib + test binary (detectable failure: compilation)
#   test-run-<cfg>     run the test binary   (detectable failure: runtime)
#   test-<cfg>         convenience alias = build + run, verbose output
#
# test-all uses test-build-* and test-run-* separately so it can
# distinguish "compiles but fails" from "does not compile".
define TEST_CFG_RULE
test-build-$(1):
	@echo "==> [1/3] Building libhajc.a with: $$($(1)_CFLAGS)"
	@$$(MAKE) -s -C $$(HAJ_ROOT) fclean >/dev/null 2>&1 || true
	@$$(MAKE) -s -C $$(HAJ_ROOT) HAJ_EXTRA_CFLAGS="$$($(1)_CFLAGS)" -j$(HAJ_JOBS)
	@echo "==> [2/3] Building test_$(1)"
	@mkdir -p $(BINDIR)
	@$$(CC) $$(TEST_CPPFLAGS) $$(TEST_CFLAGS) $$($(1)_CFLAGS) \
	    -o $(BINDIR)/test_$(1) test.c \
	    $$(TEST_LDLIBS) $$(TEST_LDFLAGS)

test-run-$(1): test-build-$(1)
	@echo "==> [3/3] Running test_$(1)"
	@./$(BINDIR)/test_$(1)

test-$(1): test-build-$(1) test-run-$(1)
endef

$(foreach cfg,$(CONFIGS),$(eval $(call TEST_CFG_RULE,$(cfg))))





LEAK_FLAGS := -DHMALLOC_LEAK_TRACE=1 -fno-omit-frame-pointer

test-leak:
	@echo ""
	@echo "======================================================="
	@echo "  Leak report demo"
	@echo "======================================================="
	@echo "==> [1/3] Building libhajc.a with leak tracing"
	@$(MAKE) -C $(HAJ_ROOT) fclean >/dev/null 2>&1 || true
	$(MAKE) -C $(HAJ_ROOT) HAJ_EXTRA_CFLAGS="$(LEAK_FLAGS)"
	@mkdir -p $(BINDIR)
	@echo "==> [2/3] test_leak_nog (-O2, no -g)"
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS:-g=) $(LEAK_FLAGS) \
	    -o $(BINDIR)/test_leak_nog leak.c \
	    $(TEST_LDLIBS) $(TEST_LDFLAGS)
	@./$(BINDIR)/test_leak_nog
	@echo ""
	@echo "==> [3/3] test_leak_g (-O0 -g)"
	$(CC) $(TEST_CPPFLAGS) $(TEST_CFLAGS) $(LEAK_FLAGS) -O0 -g \
	    -o $(BINDIR)/test_leak_g leak.c \
	    $(TEST_LDLIBS) $(TEST_LDFLAGS)
	@./$(BINDIR)/test_leak_g

clean:
	@rm -rf $(BINDIR)

fclean: clean

re: clean all
