/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * rtc.h - MC146818 real-time clock (CMOS) driver.
 */
#ifndef RAPTOR_RTC_H
#define RAPTOR_RTC_H

#include <stdint.h>

struct rtc_time {
    uint16_t year;
    uint8_t  month;   /* 1-12 */
    uint8_t  day;     /* 1-31 */
    uint8_t  hour;    /* 0-23 */
    uint8_t  minute;
    uint8_t  second;
};

void rtc_read(struct rtc_time *t);

#endif /* RAPTOR_RTC_H */
