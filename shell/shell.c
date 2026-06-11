// SPDX-License-Identifier: GPL-2.0-only
/*
 * shell.c - rsh, the Raptor shell.
 *
 * A deliberately small read-eval-print loop in the spirit of the
 * earliest Unix shells: read a line, split it into words, look the
 * first word up in the command table, run it. Two conveniences are
 * built in because they pull more than their weight:
 *
 *   - output redirection ("ls /etc > /tmp/listing", ">>" appends),
 *     implemented by capturing console output into a buffer and
 *     writing it to the ramfs;
 *   - an in-memory history ring, browsable with the `history` command.
 *
 * There is no job control, no environment, no scripting. Those want a
 * userspace, and a userspace wants processes - see docs/ROADMAP.md.
 */

#include <raptor/console.h>
#include <raptor/mm.h>
#include <raptor/ramfs.h>
#include <raptor/shell.h>
#include <raptor/string.h>

#define CAPTURE_MAX 16384   /* redirection buffer size */

struct ramfs_node *shell_cwd;

static char history_ring[SHELL_HISTORY_MAX][SHELL_LINE_MAX];
static int  history_total;

const char *shell_history(int index)
{
    int stored = shell_history_count();

    if (index < 0 || index >= stored)
        return NULL;

    int first = history_total - stored;
    return history_ring[(first + index) % SHELL_HISTORY_MAX];
}

int shell_history_count(void)
{
    return history_total < SHELL_HISTORY_MAX ?
           history_total : SHELL_HISTORY_MAX;
}

static void history_add(const char *line)
{
    strlcpy(history_ring[history_total % SHELL_HISTORY_MAX], line,
            SHELL_LINE_MAX);
    history_total++;
}

static void print_prompt(void)
{
    char path[RAMFS_PATH_MAX];

    ramfs_path(shell_cwd, path, sizeof(path));
    console_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    console_write("root@raptor");
    console_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    console_putc(':');
    console_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    console_write(path);
    console_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    console_write("# ");
}

/* Erase the current line on screen and show `text` in its place. */
static size_t replace_line(char *buf, size_t cap, size_t len,
                           const char *text)
{
    while (len--)
        console_write("\b \b");
    strlcpy(buf, text, cap);
    console_write(buf);
    return strlen(buf);
}

static void read_line(char *buf, size_t cap)
{
    size_t len = 0;
    int browse = shell_history_count();  /* one past the newest entry */

    for (;;) {
        int c = console_getchar();

        if (c == '\n') {
            console_putc('\n');
            buf[len] = '\0';
            return;
        }
        if (c == '\b') {
            if (len > 0) {
                len--;
                console_write("\b \b");
            }
            continue;
        }
        if (c == KEY_UP) {
            if (browse > 0) {
                browse--;
                len = replace_line(buf, cap, len, shell_history(browse));
            }
            continue;
        }
        if (c == KEY_DOWN) {
            if (browse < shell_history_count()) {
                browse++;
                len = replace_line(buf, cap, len,
                        browse == shell_history_count() ?
                        "" : shell_history(browse));
            }
            continue;
        }
        if (c == '\t')
            c = ' ';
        if (c < 0x20 || c > 0x7e)
            continue;                   /* ignore other control chars */
        if (len + 1 < cap) {
            buf[len++] = (char)c;
            console_putc((char)c);
        }
    }
}

/* Split a line into words in place; double quotes group words. */
static int tokenize(char *line, char **argv, int max)
{
    int argc = 0;
    char *p = line;

    while (*p) {
        while (*p == ' ')
            p++;
        if (!*p || argc >= max)
            break;

        if (*p == '"') {
            argv[argc++] = ++p;
            while (*p && *p != '"')
                p++;
        } else {
            argv[argc++] = p;
            while (*p && *p != ' ')
                p++;
        }
        if (*p)
            *p++ = '\0';
    }
    return argc;
}

static const struct shell_command *find_command(const char *name)
{
    for (const struct shell_command *c = shell_commands; c->name; c++) {
        if (strcmp(c->name, name) == 0)
            return c;
    }
    return NULL;
}

static void redirect_to_file(const char *path, const char *data, size_t len,
                             bool append)
{
    struct ramfs_node *f = ramfs_resolve(shell_cwd, path);

    if (!f)
        f = ramfs_create(shell_cwd, path, RAMFS_FILE);
    if (!f || f->type != RAMFS_FILE) {
        kprintf("rsh: cannot write to %s\n", path);
        return;
    }
    if (!ramfs_write(f, data, len, append))
        kprintf("rsh: write to %s failed\n", path);
}

static void execute(char *line)
{
    char *argv[SHELL_ARGV_MAX];
    int argc = tokenize(line, argv, SHELL_ARGV_MAX);

    if (argc == 0)
        return;

    /* Detect a trailing "> file" or ">> file". */
    const char *redirect_path = NULL;
    bool append = false;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0 || strcmp(argv[i], ">>") == 0) {
            if (i + 1 >= argc) {
                console_write("rsh: missing redirection target\n");
                return;
            }
            append = argv[i][1] == '>';
            redirect_path = argv[i + 1];
            argc = i;
            break;
        }
    }
    if (argc == 0)
        return;

    const struct shell_command *cmd = find_command(argv[0]);
    if (!cmd) {
        kprintf("rsh: %s: command not found (try 'help')\n", argv[0]);
        return;
    }

    if (!redirect_path) {
        cmd->run(argc, argv);
        return;
    }

    char *buf = kmalloc(CAPTURE_MAX);
    if (!buf) {
        console_write("rsh: out of memory for redirection\n");
        return;
    }
    console_capture_begin(buf, CAPTURE_MAX);
    cmd->run(argc, argv);
    size_t len = console_capture_end();
    redirect_to_file(redirect_path, buf, len, append);
    kfree(buf);
}

void shell_run(void)
{
    char line[SHELL_LINE_MAX];

    shell_cwd = ramfs_root();

    /* Greet the user with the message of the day. */
    struct ramfs_node *motd = ramfs_resolve(shell_cwd, "/etc/motd");
    if (motd && motd->type == RAMFS_FILE) {
        for (size_t i = 0; i < motd->size; i++)
            console_putc(motd->data[i]);
        console_putc('\n');
    }

    for (;;) {
        print_prompt();
        read_line(line, sizeof(line));
        if (line[0])
            history_add(line);
        execute(line);
    }
}
