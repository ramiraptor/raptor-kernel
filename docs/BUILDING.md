# Building and running Raptor

## Prerequisites

Raptor builds on any Linux environment with a multilib-capable GCC. No
cross-compiler is required: the kernel is compiled with `-m32
-ffreestanding` and linked with `ld -m elf_i386` against a custom linker
script, which produces a correct freestanding i386 ELF from a stock
x86-64 toolchain.

Debian / Ubuntu:

```sh
sudo apt install gcc gcc-multilib make binutils qemu-system-x86
```

Fedora:

```sh
sudo dnf install gcc glibc-devel.i686 make binutils qemu-system-x86
```

### Windows

Build inside WSL. Install a distribution (e.g. `wsl --install -d Debian`),
then run the apt line above inside it. The repository can live on the
Windows filesystem; from WSL it is visible under `/mnt/c/...`:

```powershell
wsl -d Debian
cd /mnt/c/path/to/raptor-kernel
make run-tty
```

`make run-tty` needs no graphical support at all. For `make run` (a QEMU
window), recent WSL versions display it automatically via WSLg.

## Build targets

| Target | Effect |
|---|---|
| `make` | build `build/raptor.elf` |
| `make run` | boot the kernel in a QEMU window (serial mirrored to the terminal) |
| `make run-tty` | boot headless; the serial console *is* your terminal |
| `make clean` | delete the `build/` directory |

The build is incremental (object files and dependency files land in
`build/`, mirroring the source layout) and takes well under ten seconds
from clean on any modern machine.

## Running

```sh
make run
```

QEMU loads `build/raptor.elf` directly through its built-in Multiboot
loader — no disk image, no GRUB, no install step. Within a second you get
the boot log and the `root@raptor:/#` prompt. Type `help`.

To quit: run `poweroff` inside the shell, or press `Ctrl+A X` in
`run-tty` mode, or just close the window.

### Headless / scripted runs

Because the serial port is a full console, you can drive the kernel from
a script — useful for smoke tests:

```sh
( sleep 2
  printf 'uname -a\n'
  printf 'ls /etc\n'
  printf 'poweroff\n'
  sleep 2 ) | qemu-system-i386 -m 128M -kernel build/raptor.elf \
                  -display none -serial stdio -monitor none
```

`poweroff` uses the ACPI port QEMU emulates, so the VM exits cleanly and
the script terminates on its own.

### Booting with GRUB instead of QEMU's loader

The image is standard Multiboot, so any GRUB2 can boot it:

```
menuentry "Raptor" {
    multiboot /boot/raptor.elf
}
```

## Debugging with GDB

QEMU has a built-in GDB stub:

```sh
qemu-system-i386 -m 128M -kernel build/raptor.elf -s -S -serial stdio
```

`-s` listens on `localhost:1234`, `-S` freezes the CPU at reset. In a
second terminal:

```sh
gdb build/raptor.elf
(gdb) target remote :1234
(gdb) break kmain
(gdb) continue
```

The kernel is built with `-g`, so source-level stepping, breakpoints on
any function, and inspection of kernel data structures all work.

## Troubleshooting

- **`Error loading uncompressed kernel without PVH ELF Note`** — QEMU did
  not find the Multiboot header in the first 8 KiB of the file. This
  happens if the `.multiboot` section loses its allocatable flag or is
  reordered; check `readelf -S build/raptor.elf` shows it at the lowest
  file offset of the loadable sections.
- **`gcc: error: ... -m32` or missing `bits/...` headers** — install
  `gcc-multilib` (Debian/Ubuntu) or the 32-bit glibc development package
  for your distribution.
- **No output in `run-tty`** — make sure nothing else grabbed the
  terminal; QEMU's `-serial stdio` must be the foreground process.
