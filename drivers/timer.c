// SPDX-License-Identifier: GPL-2.0-only
/*
 * timer.c - Intel 8253/8254 programmable interval timer.
 *
 * Channel 0 is wired to IRQ0. We program it in rate-generator mode to
 * fire TIMER_HZ times per second and simply count ticks. The tick
 * counter is the kernel's only notion of monotonic time and also
 * drives timer_sleep_ms(), which halts the CPU between interrupts
 * instead of spinning.
 */

#include <raptor/interrupts.h>
#include <raptor/io.h>
#include <raptor/timer.h>

#define PIT_BASE_HZ 1193182u
#define PIT_CH0     0x40
#define PIT_CMD     0x43

static volatile uint64_t ticks;

static void timer_irq(struct registers *regs)
{
    (void)regs;
    ticks++;
}

void timer_init(void)
{
    uint16_t divisor = (uint16_t)(PIT_BASE_HZ / TIMER_HZ);

    outb(PIT_CMD, 0x36);                      /* ch0, lo/hi, mode 3 */
    outb(PIT_CH0, (uint8_t)(divisor & 0xff));
    outb(PIT_CH0, (uint8_t)(divisor >> 8));

    irq_install_handler(0, timer_irq);
}

uint64_t timer_ticks(void)
{
    return ticks;
}

uint64_t timer_uptime_ms(void)
{
    return ticks * (1000 / TIMER_HZ);
}

void timer_sleep_ms(uint64_t ms)
{
    uint64_t end = ticks + (ms * TIMER_HZ + 999) / 1000;

    while (ticks < end)
        __asm__ volatile("sti; hlt");
}
