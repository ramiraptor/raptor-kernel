/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * interrupts.h - GDT, IDT, exception and IRQ handling.
 */
#ifndef RAPTOR_INTERRUPTS_H
#define RAPTOR_INTERRUPTS_H

#include <stdint.h>

/*
 * CPU state pushed by the assembly interrupt stubs (kernel/isr.S),
 * in the exact order it appears on the stack.
 */
struct registers {
    uint32_t ds;                                      /* saved data segment */
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  /* from pusha         */
    uint32_t int_no, err_code;                        /* pushed by stub     */
    uint32_t eip, cs, eflags, useresp, ss;            /* pushed by the CPU  */
};

typedef void (*irq_handler_t)(struct registers *regs);

void gdt_init(void);
void idt_init(void);

/* Register a handler for hardware IRQ line 0-15. */
void irq_install_handler(int irq, irq_handler_t handler);

#endif /* RAPTOR_INTERRUPTS_H */
