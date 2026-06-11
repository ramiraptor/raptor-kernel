// SPDX-License-Identifier: GPL-2.0-only
/*
 * rtc.c - read the battery-backed real-time clock out of CMOS.
 *
 * The MC146818 exposes its registers through the index/data pair at
 * ports 0x70/0x71. Values may be stored in BCD and/or 12-hour format
 * depending on status register B, so we normalize both. To avoid
 * reading mid-update we wait for the update-in-progress flag to clear
 * and read twice until the values agree.
 */

#include <raptor/io.h>
#include <raptor/rtc.h>
#include <raptor/string.h>

#define CMOS_INDEX 0x70
#define CMOS_DATA  0x71

static uint8_t cmos_read(uint8_t reg)
{
    outb(CMOS_INDEX, reg);
    return inb(CMOS_DATA);
}

static int update_in_progress(void)
{
    return cmos_read(0x0A) & 0x80;
}

static void read_raw(struct rtc_time *t)
{
    while (update_in_progress())
        ;
    t->second = cmos_read(0x00);
    t->minute = cmos_read(0x02);
    t->hour   = cmos_read(0x04);
    t->day    = cmos_read(0x07);
    t->month  = cmos_read(0x08);
    t->year   = cmos_read(0x09);
}

static uint8_t bcd_to_bin(uint8_t v)
{
    return (uint8_t)((v & 0x0f) + (v >> 4) * 10);
}

void rtc_read(struct rtc_time *t)
{
    struct rtc_time prev;

    read_raw(t);
    do {
        prev = *t;
        read_raw(t);
    } while (memcmp(&prev, t, sizeof(prev)) != 0);

    uint8_t status_b = cmos_read(0x0B);

    if (!(status_b & 0x04)) {          /* BCD mode */
        t->second = bcd_to_bin(t->second);
        t->minute = bcd_to_bin(t->minute);
        t->hour   = (uint8_t)(bcd_to_bin(t->hour & 0x7f) | (t->hour & 0x80));
        t->day    = bcd_to_bin(t->day);
        t->month  = bcd_to_bin(t->month);
        t->year   = bcd_to_bin((uint8_t)t->year);
    }

    if (!(status_b & 0x02) && (t->hour & 0x80))   /* 12-hour clock */
        t->hour = (uint8_t)(((t->hour & 0x7f) + 12) % 24);

    t->year += 2000;
}
