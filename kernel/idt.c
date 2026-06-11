// SPDX-License-Identifier: GPL-2.0-only
/*
 * idt.c - interrupt descriptor table, PIC remapping and dispatch.
 *
 * The first 32 vectors are CPU exceptions; an unhandled one is fatal
 * and turns into a panic with a register dump. The legacy 8259 PICs
 * reset with their IRQs mapped on top of those exception vectors (a
 * design mistake IBM made in 1981 that everyone has been working
 * around since), so we remap them to vectors 32-47 before enabling
 * interrupts.
 */

#include <stddef.h>
#include <stdint.h>
#include <raptor/interrupts.h>
#include <raptor/io.h>
#include <raptor/panic.h>

#define IDT_ENTRIES 256
#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1
#define PIC_EOI     0x20

struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   ip;

static irq_handler_t irq_handlers[16];

/* Stubs generated in isr.S. */
#define DECL_ISR(n) extern void isr##n(void);
#define DECL_IRQ(n) extern void irq##n(void);
DECL_ISR(0)  DECL_ISR(1)  DECL_ISR(2)  DECL_ISR(3)
DECL_ISR(4)  DECL_ISR(5)  DECL_ISR(6)  DECL_ISR(7)
DECL_ISR(8)  DECL_ISR(9)  DECL_ISR(10) DECL_ISR(11)
DECL_ISR(12) DECL_ISR(13) DECL_ISR(14) DECL_ISR(15)
DECL_ISR(16) DECL_ISR(17) DECL_ISR(18) DECL_ISR(19)
DECL_ISR(20) DECL_ISR(21) DECL_ISR(22) DECL_ISR(23)
DECL_ISR(24) DECL_ISR(25) DECL_ISR(26) DECL_ISR(27)
DECL_ISR(28) DECL_ISR(29) DECL_ISR(30) DECL_ISR(31)
DECL_IRQ(0)  DECL_IRQ(1)  DECL_IRQ(2)  DECL_IRQ(3)
DECL_IRQ(4)  DECL_IRQ(5)  DECL_IRQ(6)  DECL_IRQ(7)
DECL_IRQ(8)  DECL_IRQ(9)  DECL_IRQ(10) DECL_IRQ(11)
DECL_IRQ(12) DECL_IRQ(13) DECL_IRQ(14) DECL_IRQ(15)

static const char *exception_names[32] = {
    "Division by zero", "Debug", "Non-maskable interrupt", "Breakpoint",
    "Overflow", "Bound range exceeded", "Invalid opcode",
    "Device not available", "Double fault", "Coprocessor segment overrun",
    "Invalid TSS", "Segment not present", "Stack-segment fault",
    "General protection fault", "Page fault", "Reserved",
    "x87 floating-point exception", "Alignment check", "Machine check",
    "SIMD floating-point exception", "Virtualization exception",
    "Control protection exception", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Hypervisor injection exception",
    "VMM communication exception", "Security exception", "Reserved",
};

static void idt_set(int n, void (*handler)(void))
{
    uint32_t base = (uint32_t)handler;

    idt[n].base_low  = (uint16_t)(base & 0xffff);
    idt[n].base_high = (uint16_t)(base >> 16);
    idt[n].selector  = 0x08;            /* kernel code segment   */
    idt[n].zero      = 0;
    idt[n].flags     = 0x8e;            /* present, ring 0, gate */
}

static void pic_remap(void)
{
    outb(PIC1_CMD, 0x11);  io_wait();   /* start init, expect ICW4 */
    outb(PIC2_CMD, 0x11);  io_wait();
    outb(PIC1_DATA, 32);   io_wait();   /* master offset: vector 32 */
    outb(PIC2_DATA, 40);   io_wait();   /* slave offset: vector 40  */
    outb(PIC1_DATA, 0x04); io_wait();   /* slave on IRQ2            */
    outb(PIC2_DATA, 0x02); io_wait();
    outb(PIC1_DATA, 0x01); io_wait();   /* 8086 mode                */
    outb(PIC2_DATA, 0x01); io_wait();
    outb(PIC1_DATA, 0x00);              /* unmask everything        */
    outb(PIC2_DATA, 0x00);
}

void idt_init(void)
{
    ip.limit = sizeof(idt) - 1;
    ip.base  = (uint32_t)&idt;

#define SET_ISR(n) idt_set(n, isr##n);
#define SET_IRQ(n) idt_set(32 + (n), irq##n);
    SET_ISR(0)  SET_ISR(1)  SET_ISR(2)  SET_ISR(3)
    SET_ISR(4)  SET_ISR(5)  SET_ISR(6)  SET_ISR(7)
    SET_ISR(8)  SET_ISR(9)  SET_ISR(10) SET_ISR(11)
    SET_ISR(12) SET_ISR(13) SET_ISR(14) SET_ISR(15)
    SET_ISR(16) SET_ISR(17) SET_ISR(18) SET_ISR(19)
    SET_ISR(20) SET_ISR(21) SET_ISR(22) SET_ISR(23)
    SET_ISR(24) SET_ISR(25) SET_ISR(26) SET_ISR(27)
    SET_ISR(28) SET_ISR(29) SET_ISR(30) SET_ISR(31)
    SET_IRQ(0)  SET_IRQ(1)  SET_IRQ(2)  SET_IRQ(3)
    SET_IRQ(4)  SET_IRQ(5)  SET_IRQ(6)  SET_IRQ(7)
    SET_IRQ(8)  SET_IRQ(9)  SET_IRQ(10) SET_IRQ(11)
    SET_IRQ(12) SET_IRQ(13) SET_IRQ(14) SET_IRQ(15)

    pic_remap();

    __asm__ volatile("lidt %0" : : "m"(ip));
    __asm__ volatile("sti");
}

void irq_install_handler(int irq, irq_handler_t handler)
{
    if (irq >= 0 && irq < 16)
        irq_handlers[irq] = handler;
}

/* Called from isr.S for vectors 0-31. */
void isr_handler(struct registers *regs)
{
    const char *name = regs->int_no < 32 ?
            exception_names[regs->int_no] : "Unknown";

    panic_with_regs(name, regs);
}

/* Called from isr.S for vectors 32-47. */
void irq_handler(struct registers *regs)
{
    int irq = (int)regs->int_no - 32;

    if (irq >= 0 && irq < 16 && irq_handlers[irq])
        irq_handlers[irq](regs);

    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}
