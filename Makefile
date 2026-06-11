# =============================================================================
#  Raptor kernel build system
#
#  Build from a Linux environment (native or WSL) with gcc multilib support:
#      make            build build/raptor.elf
#      make run        boot the kernel in QEMU (graphical window)
#      make run-tty    boot headless, console on stdio (great over SSH/CI)
#      make clean      remove build artifacts
#
#  SPDX-License-Identifier: GPL-2.0-only
# =============================================================================

CC      := gcc
LD      := ld
QEMU    := qemu-system-i386

BUILD   := build
KERNEL  := $(BUILD)/raptor.elf

CFLAGS  := -m32 -std=gnu11 -ffreestanding -fno-builtin -fno-stack-protector \
           -fno-pic -fno-pie -fno-asynchronous-unwind-tables \
           -mno-mmx -mno-sse -mno-sse2 -msoft-float \
           -Wall -Wextra -O2 -g -Iinclude -MMD -MP
ASFLAGS := -m32 -ffreestanding -fno-pic -fno-pie
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib

# libgcc provides 64-bit arithmetic helpers (__udivdi3 and friends)
# that gcc emits calls to even in freestanding code.
LIBGCC  := $(shell $(CC) -m32 -print-libgcc-file-name)

CSRC := $(wildcard kernel/*.c drivers/*.c mm/*.c fs/*.c lib/*.c shell/*.c)
SSRC := $(wildcard boot/*.S kernel/*.S)
OBJS := $(patsubst %.c,$(BUILD)/%.o,$(CSRC)) \
        $(patsubst %.S,$(BUILD)/%.o,$(SSRC))
DEPS := $(OBJS:.o=.d)

QEMU_FLAGS := -m 128M -kernel $(KERNEL) \
              -device isa-debug-exit,iobase=0xf4,iosize=0x04

.PHONY: all run run-tty test clean

all: $(KERNEL)

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBGCC)
	@echo "Kernel image ready: $@"

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

run: $(KERNEL)
	$(QEMU) $(QEMU_FLAGS) -serial stdio

run-tty: $(KERNEL)
	$(QEMU) $(QEMU_FLAGS) -display none -serial stdio

test: $(KERNEL)
	sh tests/smoke.sh $(KERNEL)

clean:
	rm -rf $(BUILD)

-include $(DEPS)
