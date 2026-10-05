# test/sources.mk - list of source files for the hajlib test programs.
#
# Each section corresponds to a test binary. The section names are
# used by the Makefile to build object file paths.
#
# Rules:
#   - Each section lists .c files relative to this directory.
#   - Use tab indentation, one file per line, with trailing backslash.

TEST_DIR	:= .

# Framework (always compiled)
TEST_SRCS	:= \
	main.c \
	framework/test.c

# Test cases
TEST_CASES	:= \
	cases/ctype.c 

TEST_SRCS	+= $(TEST_CASES)
