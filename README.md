# Raptor

[![CI](https://github.com/ramiraptor/raptor-kernel/actions/workflows/ci.yml/badge.svg)](https://github.com/ramiraptor/raptor-kernel/actions/workflows/ci.yml)

A small, fast, monolithic Unix-like kernel for 32-bit x86, written in C and
assembly in the spirit of how Linus Torvalds started Linux in 1991: no grand
framework, no premature abstraction — just a kernel that boots on real(ish)
hardware, talks to the user, and grows one working feature at a time.

```
 ____    _    ____ _____ ___  ____
|  _ \  / \  |  _ \_   _/ _ \|  _ \
| |_) |/ _ \ | |_) || || | | | |_) |
|  _ </ ___ \|  __/ | || |_| |  _ <
|_| \_\_/  \_\_|    |_| \___/|_| \_\
```

Raptor boots from any Multiboot loader (GRUB, or QEMU's `-kernel` flag
directly), brings up interrupts, a timer, a keyboard, a serial console, a
heap and an in-memory filesystem, and drops you into **rsh** — an
interactive shell with over twenty familiar Linux-style commands.

```
Raptor 0.2.1 (Velociraptor) booting...

[ OK ] Global descriptor table installed
[ OK ] Interrupts enabled (IDT + PIC remapped to 32-47)
[ OK ] PIT timer ticking at 100 Hz
[ OK ] PS/2 keyboard ready
[ OK ] Kernel heap initialized (4 MiB)
[ OK ] Physical memory: 127 MiB detected
[ OK ] Paging enabled (128 MiB at c0000000, NULL page unmapped)
[ OK ] ramfs mounted at /

Welcome to Raptor 0.2.1 (Velociraptor)!
Type 'help' to see the available commands.

root@raptor:/# echo hello > /tmp/note
root@raptor:/# cat /tmp/note
hello
```

## Features

- **Multiboot boot protocol** — boots under GRUB or straight from
  `qemu-system-i386 -kernel`, no bootloader of its own to maintain.
- **Full interrupt plumbing** — GDT, IDT, remapped 8259 PICs, exception
  handlers that panic with a register dump instead of silently rebooting.
- **Drivers** — VGA text console, 16550 serial UART, PS/2 keyboard,
  PIT timer (100 Hz), CMOS real-time clock.
- **Dual console** — every byte of output is mirrored to the screen and
  COM1, and input is merged from the keyboard and COM1, so the same image
  runs in a window or fully headless (`make run-tty`).
- **Virtual memory** — a higher-half kernel at `0xC0000000` (the same
  split Linux uses), all RAM mapped with 4 MiB PSE pages, the NULL page
  and the entire lower 3 GiB unmapped, and page faults reported with the
  CR2 address and decoded error code.
- **Memory management** — a bitmap physical frame allocator plus a
  first-fit kernel heap with block splitting, coalescing, and corruption
  detection.
- **ramfs** — a hierarchical in-memory filesystem with absolute/relative
  path resolution (`.` and `..` included), preloaded with `/etc/motd`,
  `/etc/os-release` and friends.
- **rsh** — a shell with 30 built-in commands (`ls`, `cat`, `cd`, `mkdir`,
  `rm`, `cp`, `mv`, `grep`, `wc`, `head`, `free`, `uname`, `date`,
  `hexdump`, `lscpu`, `lspci`, ...), output redirection with `>` and
  `>>`, quoting, and arrow-key command history.

The whole thing is around 3,000 lines of code and builds in seconds.

## Quick start

You need a Linux environment (native or WSL) with 32-bit-capable GCC,
GNU make, and QEMU:

```sh
sudo apt install gcc gcc-multilib make binutils qemu-system-x86

make            # build build/raptor.elf
make run        # boot it in a QEMU window
make run-tty    # boot it headless, console on your terminal
```

Type `help` at the prompt. See [docs/BUILDING.md](docs/BUILDING.md) for
details (including Windows/WSL instructions) and
[docs/COMMANDS.md](docs/COMMANDS.md) for the full command reference.

## Source layout

```
boot/      Multiboot header and entry point (boot.S)
kernel/    core: kmain, GDT, IDT, interrupt stubs, panic
drivers/   VGA text, serial UART, PS/2 keyboard, PIT timer, RTC
mm/        physical frame allocator and kernel heap
fs/        ramfs, the in-memory filesystem
lib/       freestanding string routines, kprintf, console mux
shell/     rsh and its built-in commands
include/   public headers (one per subsystem)
docs/      architecture, building, commands, roadmap
```

Each subsystem hides behind a small header in `include/raptor/`; the
dependencies between them form a strict DAG that is spelled out in
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Philosophy

Raptor follows the way Linux itself grew:

1. **Make it boot first.** A kernel you cannot run is a kernel you cannot
   debug. Every commit leaves the tree bootable.
2. **Monolithic and proud.** Drivers live in the kernel, calls are direct,
   and the design stays obvious. (Linus won this argument in 1992.)
3. **Small pieces, sharp edges.** Every subsystem fits in one file a
   newcomer can read in one sitting. Corruption panics loudly rather than
   limping along.
4. **Real hardware semantics.** No emulation shortcuts: the PIC really is
   remapped, the PIT really is programmed, the RTC really is read with
   update-in-progress handling, because that is what the machine requires.

## Documentation

| Document | Contents |
|---|---|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | boot flow, subsystem design, memory map |
| [docs/BUILDING.md](docs/BUILDING.md) | toolchain setup, build targets, debugging with GDB |
| [docs/COMMANDS.md](docs/COMMANDS.md) | reference for every shell command |
| [docs/ROADMAP.md](docs/ROADMAP.md) | where this is going (paging, processes, syscalls) |
| [CONTRIBUTING.md](CONTRIBUTING.md) | coding style and how to send changes |

## License

Raptor is free software, released under the **GNU General Public License,
version 2** — the same license Linus Torvalds chose for Linux, for the same
reason: improvements to the kernel should flow back to everyone who runs
it. See [LICENSE](LICENSE) for the full text. Every source file carries an
`SPDX-License-Identifier: GPL-2.0-only` tag.
