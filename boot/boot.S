/*
 * boot.S - Multiboot entry point and the jump to the higher half.
 *
 * A Multiboot loader (GRUB, or QEMU's -kernel option) jumps to _start
 * in 32-bit protected mode with paging off, EAX holding the magic
 * 0x2BADB002 and EBX pointing at the multiboot info structure. The
 * kernel, however, is linked at 0xC0100000 - so before we can touch
 * any C code we must build a page directory that maps the higher half
 * onto physical memory and switch the MMU on.
 *
 * The boot page directory (4 MiB PSE pages) maps two views at once:
 *   PDE 0          : identity-maps the first 4 MiB, so the next
 *                    instruction after enabling CR0.PG still exists;
 *   PDE 768..1023  : map 0xC0000000..0xFFFFFFFF onto physical 0..1 GiB.
 *
 * After the far jump to higher_half the identity window is no longer
 * needed; mm/paging.c later installs the final page directory (built
 * from the real memory size) which drops it, unmapping NULL and the
 * rest of the lower half for good.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/* Multiboot header constants. */
.set MB_ALIGN,    1 << 0              /* align loaded modules on pages   */
.set MB_MEMINFO,  1 << 1              /* ask for the memory map          */
.set MB_FLAGS,    MB_ALIGN | MB_MEMINFO
.set MB_MAGIC,    0x1BADB002
.set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS)

.set KERNEL_VBASE, 0xC0000000

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

.section .boot, "ax", @progbits
.align 4096
boot_page_directory:
.skip 4096

.global _start
.type _start, @function
_start:
        movl    %eax, %esi          /* keep the multiboot magic safe   */

        /* PDE 0: identity-map 0-4 MiB (present | write | 4 MiB). */
        movl    $boot_page_directory, %edi
        movl    $0x00000083, (%edi)

        /* PDEs 768-1023: map the kernel window onto physical 0-1 GiB. */
        xorl    %ecx, %ecx
1:      movl    %ecx, %eax
        shll    $22, %eax
        orl     $0x83, %eax
        movl    %eax, 3072(%edi,%ecx,4)   /* 768 * 4 = 3072 */
        incl    %ecx
        cmpl    $256, %ecx
        jb      1b

        movl    %edi, %cr3

        movl    %cr4, %eax
        orl     $0x10, %eax         /* CR4.PSE: enable 4 MiB pages     */
        movl    %eax, %cr4

        movl    %cr0, %eax
        orl     $0x80000000, %eax   /* CR0.PG: paging on               */
        movl    %eax, %cr0

        /* EIP is still physical; vault over the 3 GiB line. */
        movl    $higher_half, %eax
        jmp     *%eax
.size _start, . - _start

.section .text
.type higher_half, @function
higher_half:
        movl    $stack_top, %esp    /* virtual stack                   */
        xorl    %ebp, %ebp          /* terminate stack frame walks     */

        addl    $KERNEL_VBASE, %ebx /* multiboot info: phys -> virt    */
        pushl   %ebx                /* arg 2: multiboot info pointer   */
        pushl   %esi                /* arg 1: multiboot magic          */
        call    kmain

        /* kmain should never return; if it does, park the CPU. */
1:      cli
        hlt
        jmp     1b
.size higher_half, . - higher_half
