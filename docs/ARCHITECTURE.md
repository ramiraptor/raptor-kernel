# Raptor architecture

This document describes how the kernel is put together: what happens from
the moment the bootloader jumps into our code until the shell prompt
appears, and how the subsystems relate to each other.

## Big picture

Raptor is a **monolithic kernel** for 32-bit x86 (i386), using the flat
memory model: all segments span the full 4 GiB address space, and paging
identity-maps detected RAM so virtual and physical addresses coincide.
Everything — drivers, filesystem, shell — runs in ring 0 in a single
address space with interrupts as the only source of concurrency.

That is exactly where Linux 0.01 started, and for the same reason: it is
the simplest design that produces a *usable* system, and every later
refinement (paging, processes, userspace) can be layered on top without
throwing the working system away.

## Boot flow

```
bootloader (GRUB / QEMU -kernel)
  └─ boot/boot.S      _start: set up the stack, push magic + multiboot info
       └─ kernel/main.c   kmain():
            console_init()      VGA + serial up; we can print
            gdt_init()          our own flat-model GDT, bootloader's discarded
            idt_init()          256 IDT gates, PIC remapped to vectors 32-47, sti
            timer_init()        PIT channel 0 at 100 Hz on IRQ0
            keyboard_init()     PS/2 handler on IRQ1
            kheap_init()        4 MiB heap right after the kernel image
            pmm_init()          frame bitmap built from the Multiboot memory info
            paging_init()       all RAM identity-mapped with 4 MiB pages, CR0.PG set
            ramfs_init()        root filesystem populated on the heap
            shell_run()         REPL; never returns
```

The Multiboot specification is what lets us skip writing a bootloader: any
compliant loader leaves the CPU in 32-bit protected mode with a magic
number in `EAX` and a pointer to a structure describing the machine
(memory size, command line, ...) in `EBX`. The header that marks the
binary as Multiboot-capable lives in its own section, placed first by
`linker.ld`, because the spec requires it within the first 8 KiB of the
image.

**Initialization order is a dependency order.** The console comes first so
every later step can report; the GDT/IDT must precede anything that takes
interrupts; the heap must exist before the PMM (whose bitmap reserves the
heap's frames) and before ramfs (whose nodes are heap allocations).

## Memory map

```
0x00000000 ─ 0x000FFFFF   legacy area: BIOS data, VGA memory at 0xB8000
0x00100000 ─ kernel_end   the kernel image (.multiboot .text .rodata .data .bss)
kernel_end ─ +4 MiB       kernel heap (first-fit free list)
above that                free physical frames, tracked by the PMM bitmap
```

`kernel_end` is exported by the linker script, page-aligned. The physical
memory manager marks everything below the end of the heap as used at boot
and hands out 4 KiB frames above it; today its only consumer is the `free`
command's statistics, but it is the foundation finer-grained paging will
allocate page tables from.

## Paging

`mm/paging.c` builds a single page directory of 4 MiB "large" pages
(PSE — Page Size Extension) that identity-maps every byte of detected
RAM, then sets `CR4.PSE`, loads `CR3` and flips `CR0.PG`. Virtual
addresses still equal physical addresses, so no other code changed when
the MMU came on — but accesses beyond RAM now fault deterministically
instead of floating on the bus, and the page-fault handler reports the
faulting address from `CR2` along with the decoded error code
(read/write, kernel/user, not-present/protection) before panicking.

The 4 MiB granularity keeps the entire mapping in one 4 KiB page
directory with no page tables to manage. Breaking the first entry into
4 KiB pages (to unmap the NULL page) and moving the kernel to the higher
half are the next steps on the roadmap.

## Interrupts

`kernel/isr.S` generates 48 entry stubs with assembler macros: vectors
0-31 (CPU exceptions) and 32-47 (hardware IRQs after remapping). Each stub
normalizes the stack into a `struct registers` (pushing a dummy error code
where the CPU does not), saves all general-purpose registers and segment
state, and calls into C:

- **Exceptions** → `isr_handler()` → panic with the exception name and a
  full register dump. There is no recovery in ring 0.
- **IRQs** → `irq_handler()` → dispatch through a 16-entry handler table
  (`irq_install_handler()`), then acknowledge the PIC(s) with EOI.

The 8259 PICs power on mapping IRQs 0-7 onto vectors 8-15 — on top of CPU
exceptions. The first thing `idt_init()` does after filling the IDT is
remap them to 32-47, the same offsets Linux uses.

## The console

`lib/console.c` multiplexes one logical console across two devices:

- **Output** goes to the VGA text screen (`drivers/vga.c`: 80×25 cells at
  `0xB8000`, hardware cursor via the CRT controller, software scrolling)
  *and* is mirrored to COM1 (`drivers/serial.c`, 115200 8N1).
- **Input** is merged from the PS/2 keyboard ring buffer (filled by IRQ1)
  and polled serial receive. While idle, `console_getchar()` executes
  `sti; hlt`, so the CPU genuinely sleeps between keystrokes.

This is why the same kernel binary works in a QEMU window, headless over
`-serial stdio`, and scripted in CI by piping commands into the serial
port.

The console also supports *capture*: the shell can redirect all output
into a memory buffer, which is how `command > file` works without the
commands knowing anything about redirection.

## Kernel heap

`mm/kheap.c` is a first-fit free-list allocator over a fixed 4 MiB region.
Each block has a header with a magic value (`"RAPT"`), its size, a free
flag and doubly linked neighbours. Allocation splits blocks when the
remainder is worth keeping; freeing coalesces with both neighbours. A bad
magic on `kmalloc`/`kfree` — overflow, double free, stray pointer — is an
immediate panic, on the theory that a loud early death beats silent
corruption every time.

## ramfs

`fs/ramfs.c` implements the filesystem as a tree of heap-allocated nodes:
directories hold a linked list of children, files hold a geometrically
grown data buffer. Path resolution handles absolute and relative paths,
`.` and `..`, and the usual lookup/create/delete/move/rename operations.
Like Linux's own ramfs there is no backing store — the tree is rebuilt
from scratch on every boot by `ramfs_init()`, which also preloads
`/etc/motd`, `/etc/os-release` and `/README.txt`.

## The shell

`shell/shell.c` is the read-eval-print loop: prompt (user, hostname and
cwd, in color), line editor with backspace handling, tokenizer with
double-quote support, history ring, and redirection. `shell/commands.c`
holds the command table — each built-in is a plain C function with the
classic `(argc, argv)` signature, so adding a command is a function plus
one table line.

## Source dependency graph

```
            boot.S
              │
            main.c ──────────────┐
              │                  │
   ┌───────┬──┴─────┬─────────┐  │
  gdt     idt     timer   keyboard
              │      │        │
              └── io.h, console
                        │
        console ── vga, serial, keyboard
              │
          kprintf, string
              │
        kheap ── pmm        (mm)
              │
            ramfs           (fs)
              │
        shell, commands     (shell)
```

Lower layers never call up. The headers in `include/raptor/` are the
contract; if a change does not alter a header, it cannot break another
subsystem.
