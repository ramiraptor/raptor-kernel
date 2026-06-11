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

/* ---- text tools -------------------------------------------------------- */

static int cmd_grep(int argc, char **argv)
{
    if (argc != 3) {
        console_write("usage: grep <pattern> <file>\n");
        return 1;
    }
    struct ramfs_node *f = want_file(argv[2]);
    if (!f)
        return 1;

    char line[SHELL_LINE_MAX];
    size_t pos = 0;
    int matches = 0;

    for (size_t i = 0; i <= f->size; i++) {
        char c = i < f->size ? f->data[i] : '\n';

        if (c == '\n') {
            line[pos] = '\0';
            if (pos && strstr(line, argv[1])) {
                kprintf("%s\n", line);
                matches++;
            }
            pos = 0;
        } else if (pos + 1 < sizeof(line)) {
            line[pos++] = c;
        }
    }
    return matches ? 0 : 1;
}

static int cmd_wc(int argc, char **argv)
{
    if (argc < 2) {
        console_write("usage: wc <file>...\n");
        return 1;
    }
    for (int i = 1; i < argc; i++) {
        struct ramfs_node *f = want_file(argv[i]);
        if (!f)
            return 1;

        unsigned lines = 0, words = 0;
        bool in_word = false;

        for (size_t j = 0; j < f->size; j++) {
            char c = f->data[j];

            if (c == '\n')
                lines++;
            if (c == ' ' || c == '\t' || c == '\n') {
                in_word = false;
            } else if (!in_word) {
                in_word = true;
                words++;
            }
        }
        kprintf("%6u %6u %6u %s\n", lines, words, (unsigned)f->size,
                argv[i]);
    }
    return 0;
}

static int cmd_head(int argc, char **argv)
{
    unsigned limit = 10;
    const char *path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc)
            limit = (unsigned)atoi_simple(argv[++i]);
        else
            path = argv[i];
    }
    if (!path) {
        console_write("usage: head [-n N] <file>\n");
        return 1;
    }
    struct ramfs_node *f = want_file(path);
    if (!f)
        return 1;

    unsigned lines = 0;
    for (size_t i = 0; i < f->size && lines < limit; i++) {
        console_putc(f->data[i]);
        if (f->data[i] == '\n')
            lines++;
    }
    return 0;
}

/* ---- hardware inspection ------------------------------------------------ */

static void cpuid(uint32_t leaf, uint32_t *a, uint32_t *b, uint32_t *c,
                  uint32_t *d)
{
    __asm__ volatile("cpuid"
                     : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                     : "a"(leaf), "c"(0));
}

static int cmd_lscpu(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint32_t a, b, c, d;

    char vendor[13];
    cpuid(0, &a, &b, &c, &d);
    memcpy(vendor + 0, &b, 4);
    memcpy(vendor + 4, &d, 4);
    memcpy(vendor + 8, &c, 4);
    vendor[12] = '\0';
    kprintf("Vendor:      %s\n", vendor);

    cpuid(0x80000000u, &a, &b, &c, &d);
    if (a >= 0x80000004u) {
        char brand[49];
        uint32_t *p = (uint32_t *)brand;

        for (uint32_t leaf = 0; leaf < 3; leaf++) {
            cpuid(0x80000002u + leaf, &a, &b, &c, &d);
            *p++ = a; *p++ = b; *p++ = c; *p++ = d;
        }
        brand[48] = '\0';
        const char *s = brand;
        while (*s == ' ')
            s++;
        kprintf("Model name:  %s\n", s);
    }

    cpuid(1, &a, &b, &c, &d);
    kprintf("Family:      %u  Model: %u  Stepping: %u\n",
            (a >> 8) & 0xf, ((a >> 4) & 0xf) | ((a >> 12) & 0xf0),
            a & 0xf);

    static const struct { uint32_t bit; const char *name; } edx_flags[] = {
        { 0, "fpu" }, { 3, "pse" }, { 4, "tsc" }, { 5, "msr" },
        { 6, "pae" }, { 9, "apic" }, { 13, "pge" }, { 15, "cmov" },
        { 23, "mmx" }, { 25, "sse" }, { 26, "sse2" },
    };
    console_write("Flags:      ");
    for (size_t i = 0; i < sizeof(edx_flags) / sizeof(edx_flags[0]); i++) {
        if (d & (1u << edx_flags[i].bit))
            kprintf(" %s", edx_flags[i].name);
    }
    if (c & 1)
        console_write(" sse3");
    console_putc('\n');
    return 0;
}

#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC

static uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off)
{
    outl(PCI_CONFIG_ADDR, 0x80000000u | ((uint32_t)bus << 16) |
                          ((uint32_t)dev << 11) | ((uint32_t)fn << 8) |
                          (off & 0xFC));
    return inl(PCI_CONFIG_DATA);
}

static const char *pci_class_name(uint8_t class_code)
{
    switch (class_code) {
    case 0x00: return "Unclassified";
    case 0x01: return "Mass storage controller";
    case 0x02: return "Network controller";
    case 0x03: return "Display controller";
    case 0x04: return "Multimedia controller";
    case 0x05: return "Memory controller";
    case 0x06: return "Bridge";
    case 0x0C: return "Serial bus controller";
    default:   return "Other";
    }
}

static int cmd_lspci(int argc, char **argv)
{
    (void)argc; (void)argv;

    for (uint8_t dev = 0; dev < 32; dev++) {
        uint32_t id = pci_read(0, dev, 0, 0x00);

        if ((id & 0xffff) == 0xffff)
            continue;                      /* no device in this slot */

        uint8_t fn_count =
            (pci_read(0, dev, 0, 0x0C) & 0x00800000u) ? 8 : 1;

        for (uint8_t fn = 0; fn < fn_count; fn++) {
            id = pci_read(0, dev, fn, 0x00);
            if ((id & 0xffff) == 0xffff)
                continue;

            uint32_t class_reg = pci_read(0, dev, fn, 0x08);

            kprintf("00:%02x.%u  %04x:%04x  %s\n",
                    dev, fn, id & 0xffff, id >> 16,
                    pci_class_name((uint8_t)(class_reg >> 24)));
        }
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
    { "grep",     "print lines of a file matching a pattern", cmd_grep    },
    { "wc",       "count lines, words and bytes of a file",  cmd_wc       },
    { "head",     "print the first lines of a file (-n N)",  cmd_head     },
    { "lscpu",    "show CPU vendor, model and features",     cmd_lscpu    },
    { "lspci",    "list devices on the PCI bus",             cmd_lspci    },
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
