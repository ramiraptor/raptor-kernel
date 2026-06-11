// SPDX-License-Identifier: GPL-2.0-only
/*
 * kheap.c - the kernel heap.
 *
 * A classic first-fit free-list allocator over a fixed 4 MiB region
 * that starts at the first page boundary after the kernel image (the
 * linker exports `kernel_end`). Each block carries a header with its
 * size, a free flag, doubly linked neighbours, and a magic value that
 * lets kfree() catch the most common forms of corruption loudly
 * instead of silently.
 *
 * Free blocks are split on allocation when the remainder is worth
 * keeping, and coalesced with both neighbours on free, so the heap
 * does not fragment under the shell's small, frequent allocations.
 */

#include <stdbool.h>
#include <raptor/mm.h>
#include <raptor/panic.h>
#include <raptor/string.h>

#define HEAP_SIZE   (4u * 1024u * 1024u)
#define BLOCK_MAGIC 0x52415054u             /* "RAPT" */
#define MIN_SPLIT   16u                     /* smallest useful payload */
#define ALIGN_UP(x, a) (((x) + (a) - 1) & ~((a) - 1))

struct block {
    uint32_t      magic;
    size_t        size;          /* payload bytes, excluding header */
    bool          free;
    struct block *next;
    struct block *prev;
};

extern uint8_t kernel_end[];     /* from linker.ld */

static struct block *heap_head;
static uint32_t      heap_start;

uint32_t kheap_end_addr(void)
{
    return heap_start + HEAP_SIZE;
}

void kheap_init(void)
{
    heap_start = ALIGN_UP((uint32_t)kernel_end, PAGE_SIZE);

    heap_head = (struct block *)heap_start;
    heap_head->magic = BLOCK_MAGIC;
    heap_head->size  = HEAP_SIZE - sizeof(struct block);
    heap_head->free  = true;
    heap_head->next  = NULL;
    heap_head->prev  = NULL;
}

static void split(struct block *b, size_t size)
{
    if (b->size < size + sizeof(struct block) + MIN_SPLIT)
        return;                  /* remainder too small to bother */

    struct block *rest = (struct block *)((uint8_t *)(b + 1) + size);
    rest->magic = BLOCK_MAGIC;
    rest->size  = b->size - size - sizeof(struct block);
    rest->free  = true;
    rest->next  = b->next;
    rest->prev  = b;
    if (rest->next)
        rest->next->prev = rest;

    b->size = size;
    b->next = rest;
}

void *kmalloc(size_t size)
{
    if (size == 0)
        return NULL;
    size = ALIGN_UP(size, 8);

    for (struct block *b = heap_head; b; b = b->next) {
        if (b->magic != BLOCK_MAGIC)
            panic("kheap: corrupted block header at %p", (void *)b);
        if (b->free && b->size >= size) {
            split(b, size);
            b->free = false;
            return b + 1;
        }
    }
    return NULL;                 /* out of heap */
}

void *kzalloc(size_t size)
{
    void *p = kmalloc(size);

    if (p)
        memset(p, 0, size);
    return p;
}

/* Merge b with its successor; both must be free and adjacent. */
static void merge_next(struct block *b)
{
    struct block *n = b->next;

    if (!n || !b->free || !n->free)
        return;
    b->size += sizeof(struct block) + n->size;
    b->next = n->next;
    if (b->next)
        b->next->prev = b;
}

void kfree(void *ptr)
{
    if (!ptr)
        return;

    struct block *b = (struct block *)ptr - 1;

    if (b->magic != BLOCK_MAGIC)
        panic("kfree: bad pointer %p", ptr);
    if (b->free)
        panic("kfree: double free of %p", ptr);

    b->free = true;
    merge_next(b);
    if (b->prev && b->prev->free)
        merge_next(b->prev);
}

void kheap_stats(size_t *total, size_t *used, size_t *largest_free)
{
    size_t t = 0, u = 0, l = 0;

    for (const struct block *b = heap_head; b; b = b->next) {
        t += b->size;
        if (!b->free)
            u += b->size;
        else if (b->size > l)
            l = b->size;
    }
    if (total)
        *total = t;
    if (used)
        *used = u;
    if (largest_free)
        *largest_free = l;
}
