/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * mm.h - memory management: physical frame accounting and the kernel heap.
 */
#ifndef RAPTOR_MM_H
#define RAPTOR_MM_H

#include <stddef.h>
#include <stdint.h>
#include <raptor/multiboot.h>

#define PAGE_SIZE 4096u

/* ---- physical memory manager (bitmap frame allocator) ---- */

void      pmm_init(const multiboot_info_t *mbi);
uint32_t  pmm_alloc_frame(void);          /* physical address, 0 on OOM */
void      pmm_free_frame(uint32_t addr);

uint32_t  pmm_total_kib(void);
uint32_t  pmm_used_kib(void);

/* ---- paging (identity map with 4 MiB pages) ---- */

void      paging_init(void);
uint32_t  paging_mapped_mib(void);

/* ---- kernel heap (first-fit free list with coalescing) ---- */

void      kheap_init(void);
void     *kmalloc(size_t size);
void     *kzalloc(size_t size);
void      kfree(void *ptr);

void      kheap_stats(size_t *total, size_t *used, size_t *largest_free);

#endif /* RAPTOR_MM_H */
