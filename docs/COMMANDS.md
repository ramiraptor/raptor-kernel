# rsh command reference

rsh is Raptor's built-in shell. It reads a line, splits it into words
(double quotes group words containing spaces), runs the named built-in,
and repeats. Two shell-level features apply to *every* command:

- **Redirection** — append `> file` to send a command's output to a file
  in the ramfs (created if needed, truncated if not), or `>> file` to
  append. The redirection operator must be surrounded by spaces.
- **History** — the last 16 non-empty lines are kept and shown by
  `history`.

Paths may be absolute (`/etc/motd`) or relative to the working directory;
`.` and `..` work as usual.

## General

| Command | Description |
|---|---|
| `help` | list every command with a one-line summary |
| `banner` | the Raptor logo and version |
| `clear` | clear the screen |
| `echo [words...]` | print its arguments; with `>`/`>>` this is how you write files |
| `history` | show the last 16 commands |
| `license` | license summary (GPL-2.0) |

## Files and directories

| Command | Description |
|---|---|
| `ls [-l] [path]` | list a directory (or name a file); `-l` adds type and size |
| `cat <file>...` | print file contents |
| `cd [dir]` | change directory; plain `cd` returns to `/` |
| `pwd` | print the working directory |
| `mkdir <dir>...` | create directories |
| `touch <file>...` | create empty files (existing files are left alone) |
| `rm [-r] <path>...` | remove files; `-r` removes directories recursively |
| `cp <src> <dst>` | copy a file; if `dst` is a directory the name is kept |
| `mv <src> <dst>` | move or rename a file or directory |
| `hexdump <file>` | hex + ASCII dump, 16 bytes per line |

Examples:

```
root@raptor:/# echo configuration value > /etc/myapp.conf
root@raptor:/# mkdir /home/user/projects
root@raptor:/# cp /etc/myapp.conf /home/user/projects
root@raptor:/# ls -l /home/user/projects
-       20  myapp.conf
root@raptor:/# hexdump /etc/myapp.conf
00000000  63 6f 6e 66 69 67 75 72  61 74 69 6f 6e 20 76 61  |configuration va|
00000010  6c 75 65 0a                                       |lue.|
```

## System information

| Command | Description |
|---|---|
| `uname [-a]` | kernel name; `-a` adds version, codename, arch, build date and compiler |
| `free` | physical memory and kernel heap usage |
| `uptime` | time since boot, plus the raw tick count |
| `date` | current date and time from the CMOS real-time clock (UTC) |
| `whoami` | always `root` — there is nobody else in ring 0 |

## Power and timing

| Command | Description |
|---|---|
| `sleep <seconds>` | pause; the CPU halts between timer ticks rather than spinning |
| `reboot` | reset the machine through the 8042 keyboard controller |
| `poweroff` | ACPI shutdown (QEMU, Bochs, VirtualBox); falls back to halting |
| `halt` | stop the CPU permanently |

## Exit codes and errors

Every command returns 0 on success and non-zero on failure, Unix style,
though nothing consumes the value yet (there is no `&&` chaining). Errors
go to the console in the familiar format:

```
root@raptor:/# cat /nope
/nope: no such file or directory
root@raptor:/# rm /etc
rm: /etc: directory not empty (use -r)
```

## Adding a command

1. Write a `static int cmd_yourname(int argc, char **argv)` in
   `shell/commands.c`.
2. Add one line to the `shell_commands` table at the bottom of the file.

That's the whole process; `help` picks it up automatically.
