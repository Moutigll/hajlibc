# tests/Makefile - build and run the hajlib test suite.
#
# Coverage:
#   make cov      build the lib with --coverage, run the tests,
#                 generate a gcov report for the lib sources only.
#   make cov-clean remove coverage data.
#
# Variables:
#   HAJ_ROOT     root of the hajlib source tree (default: ..)
#   HAJ_VERBOSE  if 1, pass -v to the test binary (default: 0)
#   HAJ_COV_DIR  where the gcov reports are written (default: coverage)

HAJ_ROOT	:= ..
HAJ_COV_DIR	?= coverage

# Include the public configuration.
include $(HAJ_ROOT)/mk/config.mk

# Paths.
TEST_DIR	:= .
BUILD_DIR	:= build

# Sources.
include sources.mk

# ----- Compiler flags -----------------------------------------------------

CPPFLAGS	:= $(HAJ_CPPFLAGS) -I$(TEST_DIR)
CFLAGS		:= $(HAJ_CFLAGS)
LDFLAGS		:= $(HAJ_LDFLAGS)
LDLIBS		:= $(HAJ_LIBS) $(COV_LDLIBS) $(HAJ_LIBS)

# ----- Object files -------------------------------------------------------

TEST_OBJS	:= $(patsubst %.c,$(BUILD_DIR)/%.o,$(TEST_SRCS))
TEST_BIN	:= $(BUILD_DIR)/hajtest

# ----- Verbosity ----------------------------------------------------------

HAJ_VERBOSE	?= 0
ifeq ($(HAJ_VERBOSE),1)
TEST_ARGS	:= -v
else
TEST_ARGS	:=
endif

# ----- Phony targets ------------------------------------------------------

.PHONY: all run clean fclean re cov cov-clean gcov help

all: $(TEST_BIN)

$(HAJ_LIB):
	@echo "==> Building libhajc.a"
	@$(MAKE) -C $(HAJ_ROOT) $(notdir $(HAJ_LIB))

$(TEST_BIN): $(TEST_OBJS) $(HAJ_LIB)
	@mkdir -p $(dir $@)
	@echo "==> Linking $@"
	@$(CC) $(TEST_OBJS) $(LDLIBS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "==> CC $<"
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: all
	@echo ""
	@echo "=== Running tests ==="
	@$(TEST_BIN) $(TEST_ARGS)

clean:
	@echo "==> Cleaning test build"
	@rm -rf $(BUILD_DIR)

fclean: clean
	@echo "==> Cleaning coverage data"
	@rm -rf $(HAJ_COV_DIR)
	@find $(HAJ_ROOT)/objs -name '*.gcda' -delete 2>/dev/null || true
	@find $(HAJ_ROOT)/objs -name '*.gcno' -delete 2>/dev/null || true

re: fclean all

# ----- Coverage -----------------------------------------------------------

# Force the library to be rebuilt with --coverage, then run the
# tests. The test binaries are NOT instrumented.
cov:
	@$(MAKE) HAJ_COV=1 _cov

_cov: cov-clean
	@echo "==> Rebuilding libhajc.a with coverage"
	@$(MAKE) -C $(HAJ_ROOT) fclean
	@$(MAKE) -C $(HAJ_ROOT) all
	@$(MAKE) fclean
	@$(MAKE) run
	@$(MAKE) gcov

# Remove coverage data only.
cov-clean:
	@echo "==> Cleaning coverage data"
	@rm -rf $(HAJ_COV_DIR)
	@find $(HAJ_ROOT)/objs -name '*.gcda' -delete 2>/dev/null || true
	@find $(HAJ_ROOT)/objs -name '*.gcno' -delete 2>/dev/null || true

# Internal: run gcov on the lib sources only.
gcov:
	@echo ""
	@echo "=== Generating coverage report (lib only) ==="
	@mkdir -p $(HAJ_COV_DIR)
	@for src in $(HAJ_ROOT)/src/**/*.c; do \
		obj=$$(echo "$$src" | sed "s|$(HAJ_ROOT)/src|$(HAJ_ROOT)/objs/src|; s|\.c$$|.o|"); \
		if [ -f "$$obj" ]; then \
			gcov -o "$$obj" "$$src" -o $(HAJ_COV_DIR) 2>/dev/null || true; \
		fi; \
	done
	@echo ""
	@echo "==> Coverage summary:"
	@grep -h '^Lines executed:' $(HAJ_COV_DIR)/*.gcov 2>/dev/null \
		| sort | uniq -c | sort -rn || true
	@echo ""
	@echo "==> Detailed reports: $(HAJ_COV_DIR)/*.gcov"

help:
	@echo "Test targets:"
	@echo "  make          build the test binary"
	@echo "  make run      build and run the tests"
	@echo "  make clean    remove build artifacts"
	@echo "  make fclean   clean + remove coverage data"
	@echo "  make re       fclean + all"
	@echo "  make cov      coverage for the lib only"
	@echo "  make cov-clean remove coverage data"
	@echo "  make help     this message"
