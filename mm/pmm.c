// SPDX-License-Identifier: GPL-2.0-only
/*
 * pmm.c - physical memory manager.
 *
 * A flat bitmap over 4 KiB frames: one bit per frame, set = in use.
 * The total amount of RAM comes from the Multiboot info structure.
 * Everything from physical 0 up to the end of the kernel heap is
 * marked used at boot (low memory, the kernel image, and the heap
 * region itself); the rest is free for future subsystems - paging
 * and userspace will be the first customers.
 */

#include <raptor/mm.h>
#include <raptor/panic.h>
#include <raptor/string.h>

/* Supports up to 1 GiB of RAM: 262144 frames -> 32 KiB of bitmap. */
#define MAX_FRAMES (1024u * 1024u * 1024u / PAGE_SIZE)

static uint32_t bitmap[MAX_FRAMES / 32];
static uint32_t total_frames;
static uint32_t used_frames;

/* Provided by mm/kheap.c: first frame past the kernel image + heap. */
extern uint32_t kheap_end_addr(void);

static void mark_used(uint32_t frame)
{
    if (!(bitmap[frame / 32] & (1u << (frame % 32)))) {
        bitmap[frame / 32] |= 1u << (frame % 32);
        used_frames++;
    }
}

void pmm_init(const multiboot_info_t *mbi)
{
    if (!(mbi->flags & MULTIBOOT_FLAG_MEM))
        panic("bootloader did not provide a memory map");

    /* mem_upper is KiB above 1 MiB; add the first megabyte back in. */
    uint32_t total_kib = 1024 + mbi->mem_upper;

    total_frames = total_kib / (PAGE_SIZE / 1024);
    if (total_frames > MAX_FRAMES)
        total_frames = MAX_FRAMES;

    /* Reserve everything up to the end of the kernel heap. */
    uint32_t reserved_end = kheap_end_addr();
    for (uint32_t addr = 0; addr < reserved_end; addr += PAGE_SIZE)
        mark_used(addr / PAGE_SIZE);
}

uint32_t pmm_alloc_frame(void)
{
    for (uint32_t f = 0; f < total_frames; f++) {
        if (!(bitmap[f / 32] & (1u << (f % 32)))) {
            mark_used(f);
            return f * PAGE_SIZE;
        }
    }
    return 0;       /* out of physical memory */
}

void pmm_free_frame(uint32_t addr)
{
    uint32_t frame = addr / PAGE_SIZE;

    if (frame < total_frames &&
        (bitmap[frame / 32] & (1u << (frame % 32)))) {
        bitmap[frame / 32] &= ~(1u << (frame % 32));
        used_frames--;
    }
}

uint32_t pmm_total_kib(void)
{
    return total_frames * (PAGE_SIZE / 1024);
}

uint32_t pmm_used_kib(void)
{
    return used_frames * (PAGE_SIZE / 1024);
}
