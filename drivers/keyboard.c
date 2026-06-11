// SPDX-License-Identifier: GPL-2.0-only
/*
 * keyboard.c - PS/2 keyboard driver.
 *
 * IRQ1 fires once per key event; we read the scancode (set 1) from
 * port 0x60, translate it through a US layout table and push the
 * resulting ASCII character into a small ring buffer that the console
 * drains. Shift and Caps Lock are tracked; everything more exotic
 * (extended E0 sequences, numpad, F-keys) is deliberately ignored.
 */

#include <stdbool.h>
#include <stdint.h>
#include <raptor/interrupts.h>
#include <raptor/io.h>
#include <raptor/keyboard.h>

#define KBD_DATA_PORT 0x60
#define BUF_SIZE      256          /* power of two */

static char     buf[BUF_SIZE];
static unsigned head;
static unsigned tail;

static bool shift;
static bool caps;

static const char map_lower[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'','`',
    0,   '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ',
};

static const char map_upper[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t','Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ',
};

static void push(char c)
{
    unsigned next = (head + 1) & (BUF_SIZE - 1);

    if (next != tail) {            /* drop the key if the buffer is full */
        buf[head] = c;
        head = next;
    }
}

static void keyboard_irq(struct registers *regs)
{
    (void)regs;
    uint8_t sc = inb(KBD_DATA_PORT);

    if (sc == 0xE0)                /* extended prefix: skip next byte too */
        return;

    if (sc & 0x80) {               /* key release */
        sc &= 0x7f;
        if (sc == 0x2A || sc == 0x36)
            shift = false;
        return;
    }

    switch (sc) {
    case 0x2A:                     /* left shift  */
    case 0x36:                     /* right shift */
        shift = true;
        return;
    case 0x3A:                     /* caps lock   */
        caps = !caps;
        return;
    }

    if (sc >= 128)
        return;

    char c;
    if (shift)
        c = map_upper[sc];
    else
        c = map_lower[sc];

    /* Caps Lock only affects letters. */
    if (caps && !shift && c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');
    else if (caps && shift && c >= 'A' && c <= 'Z')
        c = (char)(c - 'A' + 'a');

    if (c)
        push(c);
}

void keyboard_init(void)
{
    irq_install_handler(1, keyboard_irq);
}

int keyboard_read(void)
{
    if (head == tail)
        return -1;

    char c = buf[tail];
    tail = (tail + 1) & (BUF_SIZE - 1);
    return (unsigned char)c;
}
