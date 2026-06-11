/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * panic.h - the kernel's last words.
 */
#ifndef RAPTOR_PANIC_H
#define RAPTOR_PANIC_H

#include <raptor/interrupts.h>

/* Print a message (and optionally a register dump) and halt forever. */
void panic(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
void panic_with_regs(const char *reason, struct registers *regs)
        __attribute__((noreturn));

#endif /* RAPTOR_PANIC_H */
