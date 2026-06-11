/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * keyboard.h - PS/2 keyboard driver (US layout, scancode set 1).
 */
#ifndef RAPTOR_KEYBOARD_H
#define RAPTOR_KEYBOARD_H

void keyboard_init(void);

/* Non-blocking: next buffered character, or -1 if the buffer is empty. */
int  keyboard_read(void);

#endif /* RAPTOR_KEYBOARD_H */
