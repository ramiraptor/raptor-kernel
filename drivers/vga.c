// SPDX-License-Identifier: GPL-2.0-only
/*
 * vga.c - VGA text-mode driver.
 *
 * The classic 80x25 color text screen at physical 0xB8000. Each cell
 * is two bytes: the character and an attribute byte (low nibble =
 * foreground, high nibble = background). The hardware cursor is moved
 * through the CRT controller ports 0x3D4/0x3D5.
 */

#include <stdint.h>
#include <raptor/io.h>
#include <raptor/string.h>

#define VGA_MEM    ((volatile uint16_t *)0xB8000)
#define VGA_COLS   80
#define VGA_ROWS   25
#define CRTC_INDEX 0x3D4
#define CRTC_DATA  0x3D5

static int     cur_row;
static int     cur_col;
static uint8_t color = 0x07;          /* light grey on black */

void vga_clear(void);

static uint16_t cell(char c, uint8_t attr)
{
    return (uint16_t)c | ((uint16_t)attr << 8);
}

static void move_hw_cursor(void)
{
    uint16_t pos = (uint16_t)(cur_row * VGA_COLS + cur_col);

    outb(CRTC_INDEX, 14);
    outb(CRTC_DATA, (uint8_t)(pos >> 8));
    outb(CRTC_INDEX, 15);
    outb(CRTC_DATA, (uint8_t)pos);
}

static void scroll_if_needed(void)
{
    if (cur_row < VGA_ROWS)
        return;

    /* Move every line up by one and blank the last. */
    memmove((void *)VGA_MEM, (const void *)(VGA_MEM + VGA_COLS),
            (VGA_ROWS - 1) * VGA_COLS * sizeof(uint16_t));
    for (int c = 0; c < VGA_COLS; c++)
        VGA_MEM[(VGA_ROWS - 1) * VGA_COLS + c] = cell(' ', color);
    cur_row = VGA_ROWS - 1;
}

void vga_init(void)
{
    vga_clear();
}

void vga_clear(void)
{
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
        VGA_MEM[i] = cell(' ', color);
    cur_row = 0;
    cur_col = 0;
    move_hw_cursor();
}

void vga_set_color(uint8_t c)
{
    color = c;
}

uint8_t vga_get_color(void)
{
    return color;
}

void vga_putc(char c)
{
    switch (c) {
    case '\n':
        cur_col = 0;
        cur_row++;
        break;
    case '\r':
        cur_col = 0;
        break;
    case '\b':
        if (cur_col > 0) {
            cur_col--;
            VGA_MEM[cur_row * VGA_COLS + cur_col] = cell(' ', color);
        }
        break;
    case '\t':
        cur_col = (cur_col + 8) & ~7;
        if (cur_col >= VGA_COLS) {
            cur_col = 0;
            cur_row++;
        }
        break;
    default:
        VGA_MEM[cur_row * VGA_COLS + cur_col] = cell(c, color);
        if (++cur_col >= VGA_COLS) {
            cur_col = 0;
            cur_row++;
        }
        break;
    }

    scroll_if_needed();
    move_hw_cursor();
}
