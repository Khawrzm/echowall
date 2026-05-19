# HAVEN Microkernel — bare-metal AArch64 build
# Produces: microkernel.bin  (raw binary, linked at 0x08000000)
#
# Usage:
#   make                          # cross-compile (aarch64-linux-gnu-gcc)
#   make CC=gcc ARCH=host         # host build for unit tests
#   make clean

CC       ?= aarch64-linux-gnu-gcc
OBJCOPY  ?= aarch64-linux-gnu-objcopy

ARCH ?= aarch64

ifeq ($(ARCH),aarch64)
    ARCH_FLAGS := -march=armv8.2-a -ffreestanding -nostdlib
else
    ARCH_FLAGS := -DHAVEN_HOST_TEST=1
endif

CFLAGS := -std=c11 -O2 $(ARCH_FLAGS) \
          -Wall -Wextra -Wpedantic \
          -ffunction-sections -fdata-sections \
          -I.

LDFLAGS := -Wl,--entry=haven_main \
           -Wl,-Ttext=0x08000000 \
           -Wl,--gc-sections

SRCS := rv32ima.c microkernel.c
OBJS := $(SRCS:.c=.o)

ELF := microkernel.elf
BIN := microkernel.bin

.PHONY: all clean

all: $(BIN)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(ELF): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@
	@echo "[haven] microkernel.bin: $$(wc -c < $@) bytes"

clean:
	rm -f $(OBJS) $(ELF) $(BIN)
