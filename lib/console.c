// SPDX-License-Identifier: GPL-2.0-only
/*
 * console.c - unified kernel console.
 *
 * One putc to rule them all: every character goes to the VGA text
 * screen and is mirrored to COM1, unless a capture buffer is active
 * (used by the shell for `command > file` redirection).
 *
 * Input is merged from the PS/2 keyboard and the serial port so the
 * same kernel binary works in a graphical window and on a headless
 * serial console without any configuration.
 */

#include <raptor/console.h>
#include <raptor/keyboard.h>
#include <raptor/serial.h>

/* Implemented by drivers/vga.c. */
void    vga_init(void);
void    vga_clear(void);
void    vga_putc(char c);
void    vga_set_color(uint8_t color);
uint8_t vga_get_color(void);

static char  *capture_buf;
static size_t capture_cap;
static size_t capture_len;

void console_init(void)
{
    serial_init();
    vga_init();
}

void console_clear(void)
{
    vga_clear();
}

void console_set_color(enum vga_color fg, enum vga_color bg)
{
    vga_set_color((uint8_t)((bg << 4) | (fg & 0x0f)));
}

uint8_t console_get_color(void)
{
    return vga_get_color();
}

void console_putc(char c)
{
    if (capture_buf) {
        if (capture_len < capture_cap)
            capture_buf[capture_len++] = c;
        return;
    }

    vga_putc(c);
    if (c == '\n')
        serial_putc('\r');
    serial_putc(c);
}

void console_write(const char *s)
{
    while (*s)
        console_putc(*s++);
}

void console_capture_begin(char *buf, size_t cap)
{
    capture_buf = buf;
    capture_cap = cap;
    capture_len = 0;
}

size_t console_capture_end(void)
{
    capture_buf = NULL;
    return capture_len;
}

/*
 * The bytes of an ANSI escape sequence (ESC [ A for arrow-up, etc.)
 * arrive back to back; poll briefly for the next one.
 */
static int serial_read_soon(void)
{
    for (int spin = 0; spin < 100000; spin++) {
        int c = serial_read();
        if (c >= 0)
            return c;
    }
    return -1;
}

int console_getchar(void)
{
    for (;;) {
        int c = keyboard_read();

        if (c < 0) {
            c = serial_read();
            if (c >= 0) {
                if (c == '\r') {        /* serial terminals send CR    */
                    c = '\n';
                } else if (c == 0x7f) { /* DEL: treat as backspace     */
                    c = '\b';
                } else if (c == 0x1b) { /* ESC: maybe an arrow key     */
                    if (serial_read_soon() == '[') {
                        switch (serial_read_soon()) {
                        case 'A': c = KEY_UP;   break;
                        case 'B': c = KEY_DOWN; break;
                        default:  c = -1;       break;
                        }
                    } else {
                        c = -1;         /* lone ESC or unknown: drop   */
                    }
                }
            }
        }
        if (c >= 0)
            return c;

        /* Nothing pending: halt until the next interrupt. */
        __asm__ volatile("sti; hlt");
    }
}
