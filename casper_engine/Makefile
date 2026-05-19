# Casper Engine — C11 static library build
# Target: AArch64 / RK3588 (Cortex-A76 + NEON)
#
# Usage:
#   make              # build libcasper.a (cross or native AArch64)
#   make CC=aarch64-linux-gnu-gcc   # explicit cross-compiler
#   make clean
#   make test         # build + run host unit tests (x86_64 stub)

CC      ?= gcc
AR      ?= ar

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------
# Primary target: ARMv8.2-A with NEON (RK3588 Cortex-A76).
# The NEON path is selected at compile-time via HAVE_NEON.
# For host/test builds, set ARCH=host to disable NEON.
ARCH ?= aarch64

ifeq ($(ARCH),aarch64)
    ARCH_FLAGS := -march=armv8.2-a -mfpu=neon -DHAVE_NEON=1
else
    ARCH_FLAGS := -DHAVE_NEON=0
endif

CFLAGS := -std=c11 -O3 $(ARCH_FLAGS) \
          -Wall -Wextra -Wpedantic \
          -Werror=implicit-function-declaration \
          -Werror=incompatible-pointer-types \
          -ffunction-sections -fdata-sections \
          -fno-exceptions \
          -ffreestanding \
          -I src

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
SRCDIR := src
BUILDDIR := build

SRCS := $(SRCDIR)/tensor.c \
        $(SRCDIR)/sarf.c \
        $(SRCDIR)/casper_api.c

OBJS := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))

LIB  := libcasper.a

# ---------------------------------------------------------------------------
# Rules
# ---------------------------------------------------------------------------
.PHONY: all clean test

all: $(LIB)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB): $(OBJS)
	$(AR) rcs $@ $^
	@echo "[casper] Built $@ ($(shell wc -c < $@) bytes)"

clean:
	rm -rf $(BUILDDIR) $(LIB)

test: ARCH=host
test: CFLAGS += -DCASPER_TEST=1 -g -fsanitize=address,undefined
test: clean all
	$(CC) $(CFLAGS) -DCASPER_TEST=1 -o casper_test \
	    tests/test_tensor.c tests/test_sarf.c tests/test_api.c \
	    -L. -lcasper
	./casper_test
	@echo "[casper] All tests passed."
