/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * shell.h - rsh, the Raptor shell.
 */
#ifndef RAPTOR_SHELL_H
#define RAPTOR_SHELL_H

#include <raptor/ramfs.h>

#define SHELL_LINE_MAX    256
#define SHELL_ARGV_MAX    16
#define SHELL_HISTORY_MAX 16

struct shell_command {
    const char *name;
    const char *summary;     /* one-line description shown by `help` */
    int (*run)(int argc, char **argv);
};

/* Defined in shell/commands.c; NULL-name terminated. */
extern const struct shell_command shell_commands[];

/* Shared shell state used by the built-in commands. */
extern struct ramfs_node *shell_cwd;
extern const char *shell_history(int index);   /* 0 = oldest stored   */
extern int shell_history_count(void);

/* Enter the interactive read-eval-print loop; never returns. */
void shell_run(void) __attribute__((noreturn));

#endif /* RAPTOR_SHELL_H */
