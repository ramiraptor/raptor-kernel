/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * console.h - the kernel console.
 *
 * Output is mirrored to the VGA text screen and the first serial port,
 * so the kernel is equally usable in a graphical QEMU window and on a
 * headless serial terminal. Input is merged from the PS/2 keyboard and
 * the serial port.
 *
 * A small "capture" facility lets the shell redirect command output
 * into a memory buffer, which is how `command > file` is implemented.
 */
#ifndef RAPTOR_CONSOLE_H
#define RAPTOR_CONSOLE_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/* Standard 16-color VGA palette. */
enum vga_color {
    VGA_BLACK = 0,  VGA_BLUE,      VGA_GREEN,      VGA_CYAN,
    VGA_RED,        VGA_MAGENTA,   VGA_BROWN,      VGA_LIGHT_GREY,
    VGA_DARK_GREY,  VGA_LIGHT_BLUE, VGA_LIGHT_GREEN, VGA_LIGHT_CYAN,
    VGA_LIGHT_RED,  VGA_LIGHT_MAGENTA, VGA_YELLOW, VGA_WHITE,
};

void    console_init(void);
void    console_clear(void);
void    console_putc(char c);
void    console_write(const char *s);
void    console_set_color(enum vga_color fg, enum vga_color bg);
uint8_t console_get_color(void);

/* Blocking read of one character (keyboard or serial). */
char    console_getchar(void);

/* Redirect subsequent output into buf; returns bytes captured on end. */
void    console_capture_begin(char *buf, size_t cap);
size_t  console_capture_end(void);

void    kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void    kvprintf(const char *fmt, va_list ap);

#endif /* RAPTOR_CONSOLE_H */
