/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * multiboot.h - the subset of the Multiboot 0.6.96 information structure
 * that Raptor consumes. The bootloader fills this in and hands us a
 * pointer in EBX; we only care about the memory fields (flag bit 0).
 */
#ifndef RAPTOR_MULTIBOOT_H
#define RAPTOR_MULTIBOOT_H

#include <stdint.h>

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002u
#define MULTIBOOT_FLAG_MEM         (1u << 0)

typedef struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;     /* KiB of memory below 1 MiB  */
    uint32_t mem_upper;     /* KiB of memory above 1 MiB  */
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
} __attribute__((packed)) multiboot_info_t;

#endif /* RAPTOR_MULTIBOOT_H */
