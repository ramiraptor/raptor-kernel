/*
 * Raptor kernel linker script.
 *
 * The kernel is a higher-half kernel: it is loaded at physical 1 MiB
 * (the traditional home of x86 kernels) but linked to run at
 * 3 GiB + 1 MiB (0xC0100000), the same split Linux uses. Two address
 * spaces appear below:
 *
 *   - .boot keeps VMA == LMA == physical. It holds the Multiboot
 *     header (which must live in the first 8192 bytes of the image)
 *     and the early assembly that turns paging on - the only code
 *     that runs at physical addresses.
 *   - everything else is linked at KERNEL_VBASE + physical (the AT()
 *     directives keep the load addresses physical) and runs only
 *     after boot.S has mapped the higher half.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

ENTRY(_start)

KERNEL_VBASE = 0xC0000000;

SECTIONS
{
    /* ---- physical world: Multiboot header and early boot code ---- */
    . = 1M;

    .multiboot : ALIGN(4K)
    {
        KEEP(*(.multiboot))
    }

    .boot : ALIGN(4K)
    {
        *(.boot)
    }

    /* ---- virtual world: the kernel proper, in the higher half ---- */
    . = ALIGN(4K) + KERNEL_VBASE;

    .text : AT(ADDR(.text) - KERNEL_VBASE)
    {
        *(.text*)
    }

    . = ALIGN(4K);
    .rodata : AT(ADDR(.rodata) - KERNEL_VBASE)
    {
        *(.rodata*)
    }

    . = ALIGN(4K);
    .data : AT(ADDR(.data) - KERNEL_VBASE)
    {
        *(.data*)
    }

    . = ALIGN(4K);
    .bss : AT(ADDR(.bss) - KERNEL_VBASE)
    {
        *(COMMON)
        *(.bss*)
    }

    /* First free virtual byte after the kernel; the heap starts here. */
    . = ALIGN(4K);
    kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
