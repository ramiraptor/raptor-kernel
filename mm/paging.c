// SPDX-License-Identifier: GPL-2.0-only
/*
 * paging.c - turn on the MMU.
 *
 * Raptor identity-maps all detected RAM with 4 MiB "large" pages (the
 * Page Size Extension, present on every CPU since the Pentium) and
 * enables paging. Virtual addresses still equal physical addresses,
 * so nothing else in the kernel changes - but from here on:
 *
 *   - touching an address beyond RAM faults immediately instead of
 *     silently reading garbage from the bus;
 *   - NULL-page accesses can be trapped once the first 4 MiB is
 *     eventually broken into 4 KiB pages;
 *   - the path to a higher-half kernel and per-process address
 *     spaces is just "build finer page tables and switch CR3".
 *
 * Using 4 MiB pages keeps the whole mapping in a single page
 * directory: no page tables to allocate, one TLB entry per 4 MiB.
 */

#include <raptor/console.h>
#include <raptor/mm.h>

#define PDE_PRESENT  (1u << 0)
#define PDE_WRITE    (1u << 1)
#define PDE_LARGE    (1u << 7)     /* 4 MiB page (requires CR4.PSE) */

#define CR4_PSE      (1u << 4)
#define CR0_PG       (1u << 31)

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

    if (entries > 1024)
        entries = 1024;

    for (uint32_t i = 0; i < entries; i++)
        page_directory[i] = (i << 22) | PDE_PRESENT | PDE_WRITE | PDE_LARGE;
    /* Remaining entries stay zero: not present, faults on access. */

    uint32_t cr;

    __asm__ volatile("mov %%cr4, %0" : "=r"(cr));
    cr |= CR4_PSE;
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr));

    __asm__ volatile("mov %0, %%cr3" : : "r"((uint32_t)page_directory));

    __asm__ volatile("mov %%cr0, %0" : "=r"(cr));
    cr |= CR0_PG;
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr) : "memory");
}
