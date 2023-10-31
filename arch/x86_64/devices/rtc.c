// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/x86_interrupt.h"
#define CURRENT_YEAR 2023

#include "mos/device/clocksource.h"
#include "mos/printk.h"
#include "mos/tasks/schedule.h"
#include "mos/x86/devices/port.h"
#include "mos/x86/devices/rtc.h"

#include <mos/types.h>

static clocksource_t rtc_clocksource = {
    .name = "rtc",
    .list_node = LIST_NODE_INIT(rtc_clocksource),
    .frequency = 1000,
    .ticks = 0,
};

static const u8 RTC_STATUS_REG_B = 0x0B;
static const u8 CMOS_PORT_ADDRESS = 0x70;
static const u8 CMOS_PORT_DATA = 0x71;

u8 rtc_read_reg(x86_port_t reg)
{
    port_outb(CMOS_PORT_ADDRESS, reg);
    return port_inb(CMOS_PORT_DATA);
}

u8 rtc_is_update_in_progress()
{
    return rtc_read_reg(0x0A) & 0x80;
}

void rtc_read_time(timeval_t *time)
{
    u8 last_second;
    u8 last_minute;
    u8 last_hour;
    u8 last_day;
    u8 last_month;
    u8 last_year;

    while (rtc_is_update_in_progress())
        ; // Make sure an update isn't in progress
    time->second = rtc_read_reg(0x00);
    time->minute = rtc_read_reg(0x02);
    time->hour = rtc_read_reg(0x04);
    time->day = rtc_read_reg(0x07);
    time->month = rtc_read_reg(0x08);
    time->year = rtc_read_reg(0x09);

    do
    {
        last_second = time->second;
        last_minute = time->minute;
        last_hour = time->hour;
        last_day = time->day;
        last_month = time->month;
        last_year = time->year;

        while (rtc_is_update_in_progress())
            ; // Make sure an update isn't in progress
        time->second = rtc_read_reg(0x00);
        time->minute = rtc_read_reg(0x02);
        time->hour = rtc_read_reg(0x04);
        time->day = rtc_read_reg(0x07);
        time->month = rtc_read_reg(0x08);
        time->year = rtc_read_reg(0x09);
    } while ((last_second != time->second) || (last_minute != time->minute) || (last_hour != time->hour) || (last_day != time->day) || (last_month != time->month) ||
             (last_year != time->year));

#define bcd_to_binary(val) (((val) &0x0F) + ((val) / 16) * 10)

    const u8 registerB = rtc_read_reg(0x0B);
    if (!(registerB & 0x04))
    {
        // BCD to binary values
        time->second = bcd_to_binary(time->second);
        time->minute = bcd_to_binary(time->minute);
        time->hour = ((time->hour & 0x0F) + (((time->hour & 0x70) / 16) * 10)) | (time->hour & 0x80); //
        time->day = bcd_to_binary(time->day);
        time->month = bcd_to_binary(time->month);
        time->year = bcd_to_binary(time->year);
    }

    if (!(registerB & 0x02) && (time->hour & 0x80))
    {
        // 12 hour clock to 24 hour
        time->hour = ((time->hour & 0x7F) + 12) % 24;
    }

    time->year += (CURRENT_YEAR / 100) * 100;
    if (time->year < CURRENT_YEAR)
        time->year += 100;
}

void rtc_irq_handler(u32 irq)
{
    MOS_ASSERT(irq == IRQ_CMOS_RTC);
    rtc_read_reg(0x0C); // select register C and ack the interrupt
    clocksource_tick(&rtc_clocksource);
}

void rtc_init()
{
    port_outb(CMOS_PORT_ADDRESS, 0x80 | RTC_STATUS_REG_B); // select register B, and disable NMI
    const u8 val = port_inb(CMOS_PORT_DATA);               // read the current value of register B
    port_outb(CMOS_PORT_ADDRESS, 0x80 | RTC_STATUS_REG_B); // set the index again (a read will reset the index to register D)
    port_outb(CMOS_PORT_DATA, val | 0x40);                 // write the previous value ORed with 0x40. This turns on bit 6 of register B

    rtc_read_reg(0x0C); // select register C and ack the interrupt
    clocksource_register(&rtc_clocksource);
}
