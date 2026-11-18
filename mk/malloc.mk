# Makefile.42 - ft_malloc build of the hajlibc allocator (42 subject).
#
# Builds libft_malloc_$(HOSTTYPE).so and the libft_malloc.so link to it,
# with HMALLOC_FT42=1 (see include/hmConfig.h): only mmap, munmap,
# sysconf, getrlimit, write and pthread are called, TINY/SMALL zones hold
# at least 100 blocks of their largest size, LARGE blocks get their own
# mapping, and free/realloc validate every pointer.
#
# Meant to become the root Makefile of the 42 repository
#
# The default hajlibc build (root Makefile, `make malloc`) is not affected.

# ----- Target -----

ifeq ($(HOSTTYPE),)
HOSTTYPE	:= $(shell uname -m)_$(shell uname -s)
endif

NAME	:= libft_malloc_$(HOSTTYPE).so
LINK	:= libft_malloc.so
OBJDIR	:= objs/ft_malloc

# ----- Sources -----

MALLOC_DIR	:= src/stdlib/malloc
STRING_DIR	:= src/string

MALLOC_SRCS	:= \
	alloc.c \
	arena.c \
	bitmap.c \
	bits.c \
	block.c \
	heap.c \
	init.c \
	malloc.c \
	message.c \
	meta.c \
	options.c \
	page.c \
	pagemap.c \
	posix.c \
	profile.c \
	random.c \
	show.c \
	stats.c \
	theap.c \
	unix.c

# hajlibc's own string functions, linked hidden: the allocator does not
# depend on the host libc for them, and programs keep their own.
STRING_SRCS	:= \
	mem/memcpy.c \
	mem/memset.c \
	str/strlen.c \
	str/strnlen.c

MALLOC_SRCS	:= $(addprefix $(MALLOC_DIR)/,$(MALLOC_SRCS))
STRING_SRCS	:= $(addprefix $(STRING_DIR)/,$(STRING_SRCS))
ALL_SRCS	:= $(MALLOC_SRCS) $(STRING_SRCS)

MALLOC_OBJS	:= $(patsubst %.c,$(OBJDIR)/%.o,$(MALLOC_SRCS))
STRING_OBJS	:= $(patsubst %.c,$(OBJDIR)/%.o,$(STRING_SRCS))
OBJS		:= $(MALLOC_OBJS) $(STRING_OBJS)
DEPS		:= $(OBJS:.o=.d)

# ----- Flags -----

CFLAGS		:= \
	-Wall -Wextra -Werror -Wpedantic \
	-O3 \
	-fPIC \
	-fvisibility=hidden \
	-fno-semantic-interposition \
	-fno-plt \
	-flto \
	-ffreestanding \
	-fno-builtin \
	-fno-stack-protector \
	-fno-omit-frame-pointer \
	-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
DEPFLAGS	:= -MMD -MP

# The allocator is built against the host headers, the string functions
# against hajlibc's (they rely on its internal headers).
MALLOC_CPPFLAGS	:= -DHMALLOC_FT42=1 -DHMALLOC_SHARED_LIB -DHMALLOC_SHARED_LIB_EXPORT \
	-I$(MALLOC_DIR)/include
STRING_CPPFLAGS	:= -std=c11 -Iinclude -DHAJ_STRING_NO_DISPATCH

ifeq ($(shell uname -s),Darwin)
LDFLAGS		:= -flto -dynamiclib -pthread
else
LDFLAGS		:= -flto=auto -shared -pthread -Wl,--build-id=none
endif

$(MALLOC_OBJS): CPPFLAGS := $(MALLOC_CPPFLAGS) $(DEPFLAGS)
$(STRING_OBJS): CPPFLAGS := $(STRING_CPPFLAGS) $(DEPFLAGS)

# ----- Rules -----

.DEFAULT_GOAL	:= all
.PHONY: all clean fclean re

all: $(LINK)

$(NAME): $(OBJS)
	@echo "LD $@"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(LINK): $(NAME)
	@ln -sf $(NAME) $(LINK)
	@echo "LN $(LINK) -> $(NAME)"

clean:
	@rm -rf $(OBJDIR)

fclean: clean
	@rm -f $(NAME) $(LINK)

re: fclean
	@$(MAKE) -f $(firstword $(MAKEFILE_LIST)) --no-print-directory all

include mk/build.mk

-include $(DEPS)
