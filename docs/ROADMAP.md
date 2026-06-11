# Roadmap

Raptor grows the way Linux did: each release keeps the system bootable
and usable while adding one structural capability. The list below is
ordered — every item builds on the ones before it.

## 0.2 — Virtual memory

- Enable paging with identity-mapped kernel space; the PMM (already in
  place) becomes the page-frame supplier.
- Higher-half kernel at `0xC0000000`, freeing low virtual addresses for
  the future userspace.
- Page-fault handler that distinguishes kernel bugs (panic) from
  legitimate demand faults.

## 0.3 — Tasks and scheduling

- Kernel threads: a `task` structure, context switch on the timer IRQ,
  a round-robin scheduler.
- Sleep/wake primitives so the console input loop stops polling.
- `ps` becomes a real command.

## 0.4 — Userspace

- Ring 3 execution with per-process page directories.
- System calls via `int 0x80` (read, write, open, close, fork, exec,
  exit, ...).
- A tiny ELF loader; `rsh` moves out of the kernel and becomes the first
  user program, exactly as `/bin/sh` was for Unix.

## 0.5 — Real storage

- ATA PIO disk driver.
- A virtual filesystem layer so ramfs and a disk filesystem coexist
  behind one API (the current `ramfs_*` functions are already shaped
  for this).
- A simple persistent filesystem (FAT16 first — boring, documented,
  debuggable from the host).

## Beyond

- Pipes and file descriptors.
- `kmalloc` slab caches for fixed-size objects.
- SMP boot would be fun; SMP correctness is a different hobby.

## Non-goals

- POSIX completeness, networking stacks, GUIs. Raptor is for
  understanding kernels, not replacing one.
- Portability layers. It is an i386 kernel; clarity beats generality at
  this size.
