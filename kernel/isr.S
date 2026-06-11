/*
 * isr.S - low-level interrupt entry stubs.
 *
 * The CPU pushes (some of) the interrupted state and vectors here.
 * Each stub normalizes the stack layout (pushing a dummy error code
 * when the CPU did not supply one), records the vector number, saves
 * the general-purpose registers, and calls into C with a pointer to
 * the whole frame (struct registers in interrupts.h).
 *
 * Vectors  0-31: CPU exceptions  -> isr_handler
 * Vectors 32-47: remapped IRQs   -> irq_handler
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

.macro ISR_NOERR n
.global isr\n
isr\n:
        pushl   $0                  /* dummy error code */
        pushl   $\n
        jmp     isr_common
.endm

.macro ISR_ERR n
.global isr\n
isr\n:
        pushl   $\n                 /* CPU already pushed the error code */
        jmp     isr_common
.endm

.macro IRQ num, vector
.global irq\num
irq\num:
        pushl   $0
        pushl   $\vector
        jmp     irq_common
.endm

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

IRQ 0,  32
IRQ 1,  33
IRQ 2,  34
IRQ 3,  35
IRQ 4,  36
IRQ 5,  37
IRQ 6,  38
IRQ 7,  39
IRQ 8,  40
IRQ 9,  41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

.macro SAVE_AND_CALL handler
        pusha
        xorl    %eax, %eax
        movw    %ds, %ax
        pushl   %eax
        movw    $0x10, %ax          /* kernel data selector */
        movw    %ax, %ds
        movw    %ax, %es
        movw    %ax, %fs
        movw    %ax, %gs
        pushl   %esp                /* struct registers *   */
        call    \handler
        addl    $4, %esp
        popl    %eax
        movw    %ax, %ds
        movw    %ax, %es
        movw    %ax, %fs
        movw    %ax, %gs
        popa
        addl    $8, %esp            /* drop vector + error code */
        iret
.endm

isr_common:
        SAVE_AND_CALL isr_handler

irq_common:
        SAVE_AND_CALL irq_handler
