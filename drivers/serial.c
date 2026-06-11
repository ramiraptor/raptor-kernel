// SPDX-License-Identifier: GPL-2.0-only
/*
 * serial.c - 16550 UART driver for COM1 (I/O base 0x3F8).
 *
 * Configured for 115200 baud, 8 data bits, no parity, 1 stop bit.
 * Both directions are polled: transmit waits for the holding register
 * to drain, and receive is checked opportunistically from the console
 * input loop. At shell speeds this is more than enough.
 */

#include <raptor/io.h>
#include <raptor/serial.h>

#define COM1 0x3F8

#define REG_DATA        (COM1 + 0)
#define REG_INT_ENABLE  (COM1 + 1)
#define REG_FIFO_CTRL   (COM1 + 2)
#define REG_LINE_CTRL   (COM1 + 3)
#define REG_MODEM_CTRL  (COM1 + 4)
#define REG_LINE_STATUS (COM1 + 5)

#define LSR_DATA_READY  0x01
#define LSR_TX_EMPTY    0x20

void serial_init(void)
{
    outb(REG_INT_ENABLE, 0x00);    /* no UART interrupts, we poll      */
    outb(REG_LINE_CTRL, 0x80);     /* DLAB on: set divisor             */
    outb(REG_DATA, 0x01);          /* divisor 1 = 115200 baud (low)    */
    outb(REG_INT_ENABLE, 0x00);    /*                          (high)  */
    outb(REG_LINE_CTRL, 0x03);     /* DLAB off: 8N1                    */
    outb(REG_FIFO_CTRL, 0xC7);     /* enable + clear FIFOs             */
    outb(REG_MODEM_CTRL, 0x0B);    /* DTR + RTS + OUT2                 */
}

void serial_putc(char c)
{
    while (!(inb(REG_LINE_STATUS) & LSR_TX_EMPTY))
        ;
    outb(REG_DATA, (uint8_t)c);
}

int serial_read(void)
{
    if (!(inb(REG_LINE_STATUS) & LSR_DATA_READY))
        return -1;
    return inb(REG_DATA);
}
