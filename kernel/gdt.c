// SPDX-License-Identifier: GPL-2.0-only
/*
 * gdt.c - global descriptor table.
 *
 * Raptor uses the flat memory model: every segment spans the full 4 GiB
 * address space and protection is (eventually) paging's job. We still
 * need our own GDT because the one the bootloader leaves behind lives
 * in memory it is free to reclaim.
 *
 * Layout matches the Linux convention loosely:
 *   0x00 null   0x08 kernel code   0x10 kernel data
 *   0x18 user code   0x20 user data   (unused until userspace exists)
 */

#include <stdint.h>
#include <raptor/interrupts.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt[5];
static struct gdt_ptr   gp;

static void gdt_set(int i, uint32_t base, uint32_t limit,
                    uint8_t access, uint8_t gran)
{
    gdt[i].base_low    = (uint16_t)(base & 0xffff);
    gdt[i].base_mid    = (uint8_t)((base >> 16) & 0xff);
    gdt[i].base_high   = (uint8_t)((base >> 24) & 0xff);
    gdt[i].limit_low   = (uint16_t)(limit & 0xffff);
    gdt[i].granularity = (uint8_t)(((limit >> 16) & 0x0f) | (gran & 0xf0));
    gdt[i].access      = access;
}

void gdt_init(void)
{
    gp.limit = sizeof(gdt) - 1;
    gp.base  = (uint32_t)&gdt;

    gdt_set(0, 0, 0, 0, 0);                      /* mandatory null     */
    gdt_set(1, 0, 0xfffff, 0x9a, 0xc0);          /* ring 0 code        */
    gdt_set(2, 0, 0xfffff, 0x92, 0xc0);          /* ring 0 data        */
    gdt_set(3, 0, 0xfffff, 0xfa, 0xc0);          /* ring 3 code        */
    gdt_set(4, 0, 0xfffff, 0xf2, 0xc0);          /* ring 3 data        */

    __asm__ volatile(
        "lgdt %0\n\t"
        "movw $0x10, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"
        "ljmp $0x08, $1f\n\t"        /* reload CS with the new selector */
        "1:\n\t"
        : : "m"(gp) : "ax", "memory");
}
