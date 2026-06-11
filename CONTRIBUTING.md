# Contributing to Raptor

Patches are welcome. The bar is the one Linus set for early Linux: the
tree must boot after every change, and code should be obvious enough
that the next person can fix it without asking you.

## Ground rules

1. **Keep it booting.** Before sending anything, run the smoke test:

   ```sh
   make clean && make && make run-tty
   ```

   and exercise whatever you touched from the shell. A scripted check
   (pipe commands over `-serial stdio`, end with `poweroff`) is in
   docs/BUILDING.md.

2. **One change per patch.** A driver fix and a shell feature are two
   patches, even if you wrote them the same afternoon.

3. **No silent failure.** If a kernel invariant breaks, panic with a
   message that names the culprit. Returning garbage "to be safe" is how
   heisenbugs are born.

## Coding style

Raptor follows the Linux kernel style, lightly adapted:

- Tabs are rendered as 4 spaces in this tree; indentation is 4 spaces.
- `lower_snake_case` for functions and variables; types end in `_t` only
  when they are opaque handles.
- Opening braces on the same line for control flow, on their own line
  for functions.
- One file per subsystem; if a file needs a table of contents, split it.
- Comments explain *why* and *what the hardware demands*, not what the
  C statement below them does. Every file starts with a header comment
  saying what the file is for.
- Public interfaces live in `include/raptor/<subsystem>.h`; everything
  else is `static`.

## Licensing

Raptor is GPL-2.0-only. Every new source file must carry the SPDX tag as
its first line:

```c
// SPDX-License-Identifier: GPL-2.0-only
```

By submitting a change you certify the Developer Certificate of Origin
(https://developercertificate.org/) — i.e., you wrote it or have the
right to submit it under GPL-2.0. Sign your commits accordingly:

```
Signed-off-by: Your Name <you@example.com>
```

## Sending changes

Open a pull request or mail a patch generated with `git format-patch`.
A good commit message states the problem first and the solution second;
"fix stuff" will be returned with the same level of detail it offers.
