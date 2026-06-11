/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * io.h - x86 port I/O primitives.
 *
 * Every device on the legacy PC platform (PIC, PIT, PS/2, UART, CMOS)
 * is programmed through these four instructions, so they live in one
 * tiny header that everything includes.
 */
#ifndef RAPTOR_IO_H
#define RAPTOR_IO_H

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val)
{
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t val)
{
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*
 * Write to an unused port to give slow devices ~1 microsecond to settle.
 * Port 0x80 is the POST diagnostic port; writing to it is harmless.
 */
static inline void io_wait(void)
{
    outb(0x80, 0);
}

#endif /* RAPTOR_IO_H */
