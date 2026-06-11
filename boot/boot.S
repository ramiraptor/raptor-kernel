/*
 * boot.S - Multiboot entry point for the Raptor kernel.
 *
 * A Multiboot-compliant loader (GRUB, or QEMU's -kernel option) jumps
 * here in 32-bit protected mode with paging disabled, EAX holding the
 * magic value 0x2BADB002 and EBX pointing at the multiboot information
 * structure. All we have to do is set up a stack and call kmain().
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/* Multiboot header constants. */
.set MB_ALIGN,    1 << 0              /* align loaded modules on pages   */
.set MB_MEMINFO,  1 << 1              /* ask for the memory map          */
.set MB_FLAGS,    MB_ALIGN | MB_MEMINFO
.set MB_MAGIC,    0x1BADB002
.set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS)

.section .multiboot, "a", @progbits
.align 4
.long MB_MAGIC
.long MB_FLAGS
.long MB_CHECKSUM

/* 16 KiB kernel stack, 16-byte aligned per the System V ABI. */
.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

.section .text
.global _start
.type _start, @function
_start:
        movl    $stack_top, %esp
        xorl    %ebp, %ebp            /* terminate stack frame walks     */

        pushl   %ebx                  /* multiboot info pointer (arg 2)  */
        pushl   %eax                  /* multiboot magic       (arg 1)  */
        call    kmain

        /* kmain should never return; if it does, park the CPU. */
1:      cli
        hlt
        jmp     1b

.size _start, . - _start
