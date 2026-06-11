// SPDX-License-Identifier: GPL-2.0-only
/*
 * panic.c - print what went wrong, then stop.
 *
 * There is no recovery story in a kernel this small: if an exception
 * fires or an invariant breaks, the most useful thing we can do is
 * dump as much state as possible where the developer can see it and
 * halt with interrupts off.
 */

#include <stdarg.h>
#include <raptor/console.h>
#include <raptor/panic.h>

static void halt_forever(void) __attribute__((noreturn));

static void halt_forever(void)
{
    for (;;)
        __asm__ volatile("cli; hlt");
}

void panic(const char *fmt, ...)
{
    va_list ap;

    console_capture_end();             /* never die into a capture buffer */
    console_set_color(VGA_WHITE, VGA_RED);
    console_write("\n*** KERNEL PANIC ***\n");
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    console_write("\nSystem halted.\n");

    halt_forever();
}

void panic_with_regs(const char *reason, struct registers *regs)
{
    console_capture_end();
    console_set_color(VGA_WHITE, VGA_RED);
    kprintf("\n*** KERNEL PANIC: %s (vector %u, error %08x) ***\n",
            reason, regs->int_no, regs->err_code);
    kprintf("eax=%08x ebx=%08x ecx=%08x edx=%08x\n",
            regs->eax, regs->ebx, regs->ecx, regs->edx);
    kprintf("esi=%08x edi=%08x ebp=%08x esp=%08x\n",
            regs->esi, regs->edi, regs->ebp, regs->esp);
    kprintf("eip=%08x cs=%04x ds=%04x eflags=%08x\n",
            regs->eip, regs->cs, regs->ds, regs->eflags);
    console_write("System halted.\n");

    halt_forever();
}
