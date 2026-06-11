/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * serial.h - 16550 UART driver for COM1.
 *
 * The serial port doubles as a full console: output is mirrored to it
 * and characters received on it feed the same input stream as the
 * keyboard, which makes headless operation (`make run-tty`) possible.
 */
#ifndef RAPTOR_SERIAL_H
#define RAPTOR_SERIAL_H

void serial_init(void);
void serial_putc(char c);

/* Non-blocking: next received byte, or -1 if none is pending. */
int  serial_read(void);

#endif /* RAPTOR_SERIAL_H */
