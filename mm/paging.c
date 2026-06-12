// SPDX-License-Identifier: GPL-2.0-only
/*
 * paging.c - the final virtual memory layout.
 *
 * boot.S already switched the MMU on with a temporary directory that
 * maps both the first 4 MiB at its physical address (so the enabling
 * code could keep executing) and the higher half. This file builds
 * the *real* page directory from the detected memory size and loads
 * it, which establishes the layout the kernel runs with:
 *
 *   virtual                          physical
 *   0x00000000 - 0xBFFFFFFF   ->     unmapped: NULL and wild pointers
 *                                    page-fault immediately
 *   0xC0000000 - +RAM size    ->     0 .. RAM, 4 MiB PSE pages
 *
 * This is the same split Linux uses (PAGE_OFFSET = 0xC0000000): the
 * kernel owns the top gigabyte of every future address space, leaving
 * 0-3 GiB free for userspace once processes exist.
 *
 * Using 4 MiB pages keeps the whole mapping in a single page
 * directory: no page tables to allocate, one TLB entry per 4 MiB.
 */

#include <raptor/console.h>
#include <raptor/mm.h>

#define PDE_PRESENT  (1u << 0)
#define PDE_WRITE    (1u << 1)
#define PDE_LARGE    (1u << 7)     /* 4 MiB page (requires CR4.PSE) */

#define KERNEL_PDE   (KERNEL_VBASE >> 22)        /* entry 768 */

static uint32_t page_directory[1024] __attribute__((aligned(4096)));

uint32_t paging_mapped_mib(void)
{
    uint32_t n = 0;

    for (int i = 0; i < 1024; i++) {
        if (page_directory[i] & PDE_PRESENT)
            n++;
    }
    return n * 4;
}

void paging_init(void)
{
    /* One present 4 MiB entry per 4 MiB of detected RAM, rounded up. */
    uint32_t mib = (pmm_total_kib() + 1023) / 1024;
    uint32_t entries = (mib + 3) / 4;

    if (entries > 1024 - KERNEL_PDE)
        entries = 1024 - KERNEL_PDE;             /* 1 GiB window */

    for (uint32_t i = 0; i < entries; i++)
        page_directory[KERNEL_PDE + i] =
                (i << 22) | PDE_PRESENT | PDE_WRITE | PDE_LARGE;

    /*
     * Entries 0-767 stay zero: the identity window boot.S needed is
     * gone, the NULL page is unmapped, and the lower 3 GiB is held
     * in reserve for userspace. CR4.PSE and CR0.PG are already set;
     * loading CR3 with the new directory also flushes the TLB.
     */
    __asm__ volatile("mov %0, %%cr3" : : "r"(V2P(page_directory))
                     : "memory");
}
