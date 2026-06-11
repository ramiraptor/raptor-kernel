// SPDX-License-Identifier: GPL-2.0-only
/*
 * kprintf.c - minimal printf for the kernel console.
 *
 * Supports: %c %s %d %i %u %x %X %p %%, with optional '-' (left align)
 * and '0' (zero pad) flags and a decimal field width, e.g. "%08x" and
 * "%-12s". That covers everything the kernel and shell need; anything
 * fancier belongs in userspace, which we do not have yet.
 */

#include <stdbool.h>
#include <stdint.h>
#include <raptor/console.h>
#include <raptor/string.h>

static void pad(char c, int n)
{
    while (n-- > 0)
        console_putc(c);
}

static void emit(const char *s, size_t len, int width, bool left, char fill)
{
    int gap = width > (int)len ? width - (int)len : 0;

    if (!left)
        pad(fill, gap);
    while (len--)
        console_putc(*s++);
    if (left)
        pad(' ', gap);
}

/* Render an unsigned number into buf (backwards-safe) and return length. */
static size_t format_num(char *buf, unsigned int value, unsigned int base,
                         bool uppercase)
{
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[16];
    size_t n = 0;

    do {
        tmp[n++] = digits[value % base];
        value /= base;
    } while (value);

    for (size_t i = 0; i < n; i++)
        buf[i] = tmp[n - 1 - i];
    return n;
}

void kvprintf(const char *fmt, va_list ap)
{
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            console_putc(*fmt);
            continue;
        }
        fmt++;

        bool left = false;
        char fill = ' ';
        int width = 0;

        for (;; fmt++) {
            if (*fmt == '-')
                left = true;
            else if (*fmt == '0')
                fill = '0';
            else
                break;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        char buf[16];
        size_t len;

        switch (*fmt) {
        case 'c': {
            char c = (char)va_arg(ap, int);
            emit(&c, 1, width, left, ' ');
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s)
                s = "(null)";
            emit(s, strlen(s), width, left, ' ');
            break;
        }
        case 'd':
        case 'i': {
            int v = va_arg(ap, int);
            unsigned int u = v < 0 ? (unsigned int)-v : (unsigned int)v;
            len = format_num(buf + 1, u, 10, false);
            if (v < 0) {
                buf[0] = '-';
                emit(buf, len + 1, width, left, fill);
            } else {
                emit(buf + 1, len, width, left, fill);
            }
            break;
        }
        case 'u':
            len = format_num(buf, va_arg(ap, unsigned int), 10, false);
            emit(buf, len, width, left, fill);
            break;
        case 'x':
        case 'X':
            len = format_num(buf, va_arg(ap, unsigned int), 16, *fmt == 'X');
            emit(buf, len, width, left, fill);
            break;
        case 'p':
            console_write("0x");
            len = format_num(buf, (unsigned int)(uintptr_t)va_arg(ap, void *),
                             16, false);
            emit(buf, len, 8, false, '0');
            break;
        case '%':
            console_putc('%');
            break;
        case '\0':
            return;
        default:
            console_putc('%');
            console_putc(*fmt);
            break;
        }
    }
}

void kprintf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}
