// SPDX-License-Identifier: GPL-2.0-only
/*
 * commands.c - the built-in shell commands.
 *
 * Each command is a tiny C function with the classic (argc, argv)
 * signature, registered in the shell_commands table at the bottom of
 * this file. They are modeled on their Linux counterparts but only
 * implement the slice of behaviour that makes sense without users,
 * permissions or processes.
 */

#include <raptor/console.h>
#include <raptor/io.h>
#include <raptor/mm.h>
#include <raptor/ramfs.h>
#include <raptor/rtc.h>
#include <raptor/shell.h>
#include <raptor/string.h>
#include <raptor/timer.h>
#include <raptor/version.h>

static int atoi_simple(const char *s)
{
    int v = 0;

    while (*s >= '0' && *s <= '9')
        v = v * 10 + (*s++ - '0');
    return v;
}

static struct ramfs_node *want_file(const char *path)
{
    struct ramfs_node *n = ramfs_resolve(shell_cwd, path);

    if (!n)
        kprintf("%s: no such file or directory\n", path);
    else if (n->type != RAMFS_FILE)
        kprintf("%s: is a directory\n", path);
    else
        return n;
    return NULL;
}

/* ---- general ---------------------------------------------------------- */

static int cmd_help(int argc, char **argv)
{
    (void)argc; (void)argv;
    console_write("Built-in commands:\n");
    for (const struct shell_command *c = shell_commands; c->name; c++)
        kprintf("  %-10s %s\n", c->name, c->summary);
    console_write("\nRedirect any output with '>' or '>>', e.g."
                  " 'ls /etc > /tmp/listing'.\n");
    return 0;
}

static int cmd_clear(int argc, char **argv)
{
    (void)argc; (void)argv;
    console_clear();
    return 0;
}

static int cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (i > 1)
            console_putc(' ');
        console_write(argv[i]);
    }
    console_putc('\n');
    return 0;
}

static int cmd_banner(int argc, char **argv)
{
    (void)argc; (void)argv;
    console_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    console_write(
        " ____    _    ____ _____ ___  ____\n"
        "|  _ \\  / \\  |  _ \\_   _/ _ \\|  _ \\\n"
        "| |_) |/ _ \\ | |_) || || | | | |_) |\n"
        "|  _ </ ___ \\|  __/ | || |_| |  _ <\n"
        "|_| \\_\\_/  \\_\\_|    |_| \\___/|_| \\_\\\n");
    console_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf("%s %s (%s) for %s\n",
            RAPTOR_NAME, RAPTOR_VERSION, RAPTOR_CODENAME, RAPTOR_ARCH);
    return 0;
}

static int cmd_uname(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "-a") == 0)
        kprintf("%s %s %s %s (built %s, %s)\n",
                RAPTOR_NAME, RAPTOR_VERSION, RAPTOR_CODENAME, RAPTOR_ARCH,
                RAPTOR_BUILD, RAPTOR_COMPILER);
    else
        kprintf("%s\n", RAPTOR_NAME);
    return 0;
}

static int cmd_whoami(int argc, char **argv)
{
    (void)argc; (void)argv;
    console_write("root\n");
    return 0;
}

static int cmd_license(int argc, char **argv)
{
    (void)argc; (void)argv;
    console_write(
        RAPTOR_NAME " is free software released under the GNU General\n"
        "Public License, version 2 - the same license Linus Torvalds\n"
        "chose for Linux. See the LICENSE file in the source tree.\n");
    return 0;
}

static int cmd_history(int argc, char **argv)
{
    (void)argc; (void)argv;
    int count = shell_history_count();

    for (int i = 0; i < count; i++)
        kprintf("%4d  %s\n", i + 1, shell_history(i));
    return 0;
}

/* ---- filesystem -------------------------------------------------------- */

static int cmd_pwd(int argc, char **argv)
{
    (void)argc; (void)argv;
    char path[RAMFS_PATH_MAX];

    ramfs_path(shell_cwd, path, sizeof(path));
    kprintf("%s\n", path);
    return 0;
}

static int cmd_cd(int argc, char **argv)
{
    const char *target = argc > 1 ? argv[1] : "/";
    struct ramfs_node *n = ramfs_resolve(shell_cwd, target);

    if (!n) {
        kprintf("cd: %s: no such directory\n", target);
        return 1;
    }
    if (n->type != RAMFS_DIR) {
        kprintf("cd: %s: not a directory\n", target);
        return 1;
    }
    shell_cwd = n;
    return 0;
}

static int cmd_ls(int argc, char **argv)
{
    bool detail = false;
    const char *path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0)
            detail = true;
        else
            path = argv[i];
    }

    struct ramfs_node *n = ramfs_resolve(shell_cwd, path ? path : ".");
    if (!n) {
        kprintf("ls: %s: no such file or directory\n", path);
        return 1;
    }
    if (n->type == RAMFS_FILE) {
        kprintf("%s\n", n->name);
        return 0;
    }

    for (const struct ramfs_node *c = n->children; c; c = c->next) {
        if (detail) {
            kprintf("%c %8u  %s%s\n",
                    c->type == RAMFS_DIR ? 'd' : '-',
                    (unsigned)c->size, c->name,
                    c->type == RAMFS_DIR ? "/" : "");
        } else {
            if (c->type == RAMFS_DIR)
                console_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
            kprintf("%s%s  ", c->name, c->type == RAMFS_DIR ? "/" : "");
            console_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        }
    }
    if (!detail)
        console_putc('\n');
    return 0;
}

static int cmd_cat(int argc, char **argv)
{
    if (argc < 2) {
        console_write("usage: cat <file>...\n");
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        struct ramfs_node *f = want_file(argv[i]);
        if (!f)
            return 1;
        for (size_t j = 0; j < f->size; j++)
            console_putc(f->data[j]);
        if (f->size && f->data[f->size - 1] != '\n')
            console_putc('\n');
    }
    return 0;
}

static int cmd_mkdir(int argc, char **argv)
{
    if (argc < 2) {
        console_write("usage: mkdir <dir>...\n");
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        if (!ramfs_create(shell_cwd, argv[i], RAMFS_DIR)) {
            kprintf("mkdir: cannot create %s\n", argv[i]);
            return 1;
        }
    }
    return 0;
}

static int cmd_touch(int argc, char **argv)
{
    if (argc < 2) {
        console_write("usage: touch <file>...\n");
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        if (!ramfs_resolve(shell_cwd, argv[i]) &&
            !ramfs_create(shell_cwd, argv[i], RAMFS_FILE)) {
            kprintf("touch: cannot create %s\n", argv[i]);
            return 1;
        }
    }
    return 0;
}

static int cmd_rm(int argc, char **argv)
{
    bool recursive = false;
    int removed = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0) {
            recursive = true;
            continue;
        }
        struct ramfs_node *n = ramfs_resolve(shell_cwd, argv[i]);
        if (!n) {
            kprintf("rm: %s: no such file or directory\n", argv[i]);
            return 1;
        }
        if (n == shell_cwd) {
            console_write("rm: refusing to remove the working directory\n");
            return 1;
        }
        if (!ramfs_delete(n, recursive)) {
            kprintf("rm: %s: directory not empty (use -r)\n", argv[i]);
            return 1;
        }
        removed++;
    }
    if (!removed) {
        console_write("usage: rm [-r] <path>...\n");
        return 1;
    }
    return 0;
}

/* Split "a/b/name" into the containing directory node and final name. */
static struct ramfs_node *split_parent(const char *path, const char **name)
{
    const char *slash = NULL;

    for (const char *p = path; *p; p++) {
        if (*p == '/')
            slash = p;
    }
    if (!slash) {
        *name = path;
        return shell_cwd;
    }

    *name = slash + 1;

    static char dir[RAMFS_PATH_MAX];
    size_t len = (size_t)(slash - path);
    if (len == 0)
        return ramfs_root();
    if (len >= sizeof(dir))
        return NULL;
    memcpy(dir, path, len);
    dir[len] = '\0';
    return ramfs_resolve(shell_cwd, dir);
}

static int cmd_cp(int argc, char **argv)
{
    if (argc != 3) {
        console_write("usage: cp <src> <dst>\n");
        return 1;
    }
    struct ramfs_node *src = want_file(argv[1]);
    if (!src)
        return 1;

    char dstpath[RAMFS_PATH_MAX];
    strlcpy(dstpath, argv[2], sizeof(dstpath));

    /* Copying into a directory keeps the source name. */
    struct ramfs_node *dst = ramfs_resolve(shell_cwd, dstpath);
    if (dst && dst->type == RAMFS_DIR) {
        strlcpy(dstpath + strlen(dstpath), "/", sizeof(dstpath) - strlen(dstpath));
        strlcpy(dstpath + strlen(dstpath), src->name,
                sizeof(dstpath) - strlen(dstpath));
        dst = ramfs_resolve(shell_cwd, dstpath);
    }
    if (!dst)
        dst = ramfs_create(shell_cwd, dstpath, RAMFS_FILE);
    if (!dst || dst->type != RAMFS_FILE) {
        kprintf("cp: cannot create %s\n", dstpath);
        return 1;
    }
    if (!ramfs_write(dst, src->data, src->size, false)) {
        console_write("cp: out of memory\n");
        return 1;
    }
    return 0;
}

static int cmd_mv(int argc, char **argv)
{
    if (argc != 3) {
        console_write("usage: mv <src> <dst>\n");
        return 1;
    }
    struct ramfs_node *src = ramfs_resolve(shell_cwd, argv[1]);
    if (!src) {
        kprintf("mv: %s: no such file or directory\n", argv[1]);
        return 1;
    }

    struct ramfs_node *parent;
    const char *name;

    struct ramfs_node *dst = ramfs_resolve(shell_cwd, argv[2]);
    if (dst && dst->type == RAMFS_DIR) {
        parent = dst;
        name = src->name;
    } else if (dst) {
        kprintf("mv: %s: destination exists\n", argv[2]);
        return 1;
    } else {
        parent = split_parent(argv[2], &name);
        if (!parent || parent->type != RAMFS_DIR) {
            kprintf("mv: %s: no such directory\n", argv[2]);
            return 1;
        }
    }

    if (!ramfs_move(src, parent, name)) {
        console_write("mv: move failed\n");
        return 1;
    }
    return 0;
}

static int cmd_hexdump(int argc, char **argv)
{
    if (argc < 2) {
        console_write("usage: hexdump <file>\n");
        return 1;
    }
    struct ramfs_node *f = want_file(argv[1]);
    if (!f)
        return 1;

    for (size_t off = 0; off < f->size; off += 16) {
        kprintf("%08x  ", (unsigned)off);
        for (size_t i = 0; i < 16; i++) {
            if (off + i < f->size)
                kprintf("%02x ", (unsigned char)f->data[off + i]);
            else
                console_write("   ");
            if (i == 7)
                console_putc(' ');
        }
        console_write(" |");
        for (size_t i = 0; i < 16 && off + i < f->size; i++) {
            char c = f->data[off + i];
            console_putc(c >= 0x20 && c < 0x7f ? c : '.');
        }
        console_write("|\n");
    }
    return 0;
}

/* ---- system ------------------------------------------------------------ */

static int cmd_free(int argc, char **argv)
{
    (void)argc; (void)argv;
    size_t heap_total, heap_used, heap_largest;

    kheap_stats(&heap_total, &heap_used, &heap_largest);
    console_write("              total        used        free\n");
    kprintf("Mem:    %8u KiB %8u KiB %8u KiB\n",
            pmm_total_kib(), pmm_used_kib(),
            pmm_total_kib() - pmm_used_kib());
    kprintf("Heap:   %8u KiB %8u KiB %8u KiB  (largest free block %u KiB)\n",
            (unsigned)(heap_total / 1024), (unsigned)(heap_used / 1024),
            (unsigned)((heap_total - heap_used) / 1024),
            (unsigned)(heap_largest / 1024));
    return 0;
}

static int cmd_uptime(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint64_t ms = timer_uptime_ms();
    unsigned secs = (unsigned)(ms / 1000);

    kprintf("up %u:%02u:%02u (%u ticks at %u Hz)\n",
            secs / 3600, (secs / 60) % 60, secs % 60,
            (unsigned)timer_ticks(), TIMER_HZ);
    return 0;
}

static int cmd_date(int argc, char **argv)
{
    (void)argc; (void)argv;
    struct rtc_time t;

    rtc_read(&t);
    kprintf("%u-%02u-%02u %02u:%02u:%02u UTC\n",
            t.year, t.month, t.day, t.hour, t.minute, t.second);
    return 0;
}

static int cmd_sleep(int argc, char **argv)
{
    if (argc < 2) {
        console_write("usage: sleep <seconds>\n");
        return 1;
    }
    timer_sleep_ms((uint64_t)atoi_simple(argv[1]) * 1000);
    return 0;
}

static int cmd_reboot(int argc, char **argv)
{
    (void)argc; (void)argv;
    console_write("Rebooting...\n");

    /* Pulse the CPU reset line through the 8042 keyboard controller. */
    while (inb(0x64) & 0x02)
        ;
    outb(0x64, 0xFE);

    for (;;)
        __asm__ volatile("hlt");
    return 0;
}

static int cmd_poweroff(int argc, char **argv)
{
    (void)argc; (void)argv;
    console_write("Powering off...\n");

    outw(0x604, 0x2000);     /* QEMU (pc machine type)  */
    outw(0xB004, 0x2000);    /* older QEMU and Bochs    */
    outw(0x4004, 0x3400);    /* VirtualBox              */
    outb(0xf4, 0x10);        /* QEMU isa-debug-exit     */

    console_write("Power-off not supported here; halting instead.\n");
    for (;;)
        __asm__ volatile("cli; hlt");
    return 0;
}

static int cmd_halt(int argc, char **argv)
{
    (void)argc; (void)argv;
    console_write("System halted. It is now safe to close the VM.\n");
    for (;;)
        __asm__ volatile("cli; hlt");
    return 0;
}

/* ---- the registry ------------------------------------------------------ */

const struct shell_command shell_commands[] = {
    { "help",     "list all commands",                       cmd_help     },
    { "banner",   "show the Raptor logo and version",        cmd_banner   },
    { "clear",    "clear the screen",                        cmd_clear    },
    { "echo",     "print arguments (supports > and >>)",     cmd_echo     },
    { "ls",       "list directory contents (-l for sizes)",  cmd_ls       },
    { "cat",      "print file contents",                     cmd_cat      },
    { "cd",       "change the working directory",            cmd_cd       },
    { "pwd",      "print the working directory",             cmd_pwd      },
    { "mkdir",    "create directories",                      cmd_mkdir    },
    { "touch",    "create empty files",                      cmd_touch    },
    { "rm",       "remove files (-r for directories)",       cmd_rm       },
    { "cp",       "copy a file",                             cmd_cp       },
    { "mv",       "move or rename a file or directory",      cmd_mv       },
    { "hexdump",  "show a file as hex and ASCII",            cmd_hexdump  },
    { "free",     "show memory usage",                       cmd_free     },
    { "uptime",   "show time since boot",                    cmd_uptime   },
    { "date",     "show the current date and time (RTC)",    cmd_date     },
    { "sleep",    "pause for N seconds",                     cmd_sleep    },
    { "history",  "show recent commands",                    cmd_history  },
    { "uname",    "print kernel information (-a for all)",   cmd_uname    },
    { "whoami",   "print the current user",                  cmd_whoami   },
    { "license",  "show license information",                cmd_license  },
    { "reboot",   "restart the machine",                     cmd_reboot   },
    { "poweroff", "shut the machine down",                   cmd_poweroff },
    { "halt",     "stop the CPU",                            cmd_halt     },
    { NULL, NULL, NULL },
};
