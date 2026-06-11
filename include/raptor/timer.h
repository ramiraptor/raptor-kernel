/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * timer.h - programmable interval timer (PIT) driver.
 */
#ifndef RAPTOR_TIMER_H
#define RAPTOR_TIMER_H

#include <stdint.h>

#define TIMER_HZ 100   /* timer interrupt frequency: one tick every 10 ms */

void     timer_init(void);
uint64_t timer_ticks(void);
uint64_t timer_uptime_ms(void);

/* Sleep with interrupts enabled; the CPU halts between ticks. */
void     timer_sleep_ms(uint64_t ms);

#endif /* RAPTOR_TIMER_H */
