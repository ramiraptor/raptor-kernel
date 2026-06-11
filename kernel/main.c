// SPDX-License-Identifier: GPL-2.0-only
/*
 * main.c - kernel entry point.
 *
 * boot/boot.S hands control here with the Multiboot magic in the first
 * argument and the bootloader's info structure in the second. kmain()
 * brings the machine up one subsystem at a time, in strict dependency
 * order, then drops into the interactive shell. It never returns.
 *
 * Initialization order matters:
 *   console   first, so every later step can print
 *   gdt/idt   before anything that relies on interrupts
 *   timer     gives us time; keyboard gives us input
 *   kheap     before pmm (the frame bitmap reserves the heap region)
 *             and before ramfs (nodes live on the heap)
 */

#include <raptor/console.h>
#include <raptor/interrupts.h>
#include <raptor/keyboard.h>
#include <raptor/mm.h>
#include <raptor/multiboot.h>
#include <raptor/panic.h>
#include <raptor/ramfs.h>
#include <raptor/shell.h>
#include <raptor/timer.h>
#include <raptor/version.h>

static void step(const char *what)
{
    console_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    console_write("[ ");
    console_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    console_write("OK");
    console_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    console_write(" ] ");
    console_write(what);
    console_putc('\n');
}

void kmain(uint32_t magic, multiboot_info_t *mbi)
{
    console_init();

    console_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("\n%s %s (%s) booting...\n\n",
            RAPTOR_NAME, RAPTOR_VERSION, RAPTOR_CODENAME);
    console_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
        panic("not loaded by a Multiboot bootloader (magic %08x)", magic);

    gdt_init();
    step("Global descriptor table installed");

    idt_init();
    step("Interrupts enabled (IDT + PIC remapped to 32-47)");

    timer_init();
    step("PIT timer ticking at 100 Hz");

    keyboard_init();
    step("PS/2 keyboard ready");

    kheap_init();
    step("Kernel heap initialized (4 MiB)");

    pmm_init(mbi);
    kprintf("[ ");
    console_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    console_write("OK");
    console_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf(" ] Physical memory: %u MiB detected\n",
            pmm_total_kib() / 1024);

    paging_init();
    kprintf("[ ");
    console_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    console_write("OK");
    console_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf(" ] Paging enabled (%u MiB identity-mapped, 4 MiB pages)\n",
            paging_mapped_mib());

    ramfs_init();
    step("ramfs mounted at /");

    console_putc('\n');
    shell_run();
}
