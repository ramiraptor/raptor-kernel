/*
 * Raptor kernel linker script.
 *
 * The kernel is linked as a 32-bit ELF executable loaded at the 1 MiB
 * physical mark, the traditional home of x86 kernels (everything below
 * 1 MiB is a minefield of BIOS data areas and legacy MMIO).
 *
 * The .multiboot section must come first: the Multiboot specification
 * requires the header to live within the first 8192 bytes of the image.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

ENTRY(_start)

SECTIONS
{
    . = 1M;

    .multiboot : ALIGN(4K)
    {
        KEEP(*(.multiboot))
    }

    .text : ALIGN(4K)
    {
        *(.text*)
    }

    .rodata : ALIGN(4K)
    {
        *(.rodata*)
    }

    .data : ALIGN(4K)
    {
        *(.data*)
    }

    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss*)
    }

    /* First free byte after the kernel image; the heap starts here. */
    . = ALIGN(4K);
    kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
