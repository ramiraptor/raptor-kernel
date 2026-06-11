/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * ramfs.h - a hierarchical in-memory filesystem.
 *
 * Every node lives on the kernel heap. Directories keep a singly
 * linked list of children; files keep a dynamically grown data buffer.
 * Paths may be absolute ("/etc/motd") or relative to a caller-supplied
 * working directory, and "." / ".." resolve as usual.
 */
#ifndef RAPTOR_RAMFS_H
#define RAPTOR_RAMFS_H

#include <stddef.h>
#include <stdbool.h>

#define RAMFS_NAME_MAX 32
#define RAMFS_PATH_MAX 256

enum ramfs_type { RAMFS_FILE, RAMFS_DIR };

struct ramfs_node {
    char               name[RAMFS_NAME_MAX];
    enum ramfs_type    type;

    /* file payload */
    char              *data;
    size_t             size;
    size_t             capacity;

    /* tree links */
    struct ramfs_node *parent;
    struct ramfs_node *children;   /* first child (directories only) */
    struct ramfs_node *next;       /* next sibling                   */
};

void               ramfs_init(void);
struct ramfs_node *ramfs_root(void);

/* Resolve a path; returns NULL if any component is missing. */
struct ramfs_node *ramfs_resolve(struct ramfs_node *cwd, const char *path);

/* Create a file or directory; the parent directory must exist. */
struct ramfs_node *ramfs_create(struct ramfs_node *cwd, const char *path,
                                enum ramfs_type type);

/* Replace (append=false) or extend (append=true) a file's contents. */
bool ramfs_write(struct ramfs_node *file, const char *data, size_t len,
                 bool append);

/* Remove a node; directories require recursive=true unless empty. */
bool ramfs_delete(struct ramfs_node *node, bool recursive);

/* Re-attach a node under a new parent and/or new name. */
bool ramfs_move(struct ramfs_node *node, struct ramfs_node *new_parent,
                const char *new_name);

/* Build the absolute path of a node into buf. */
void ramfs_path(const struct ramfs_node *node, char *buf, size_t len);

#endif /* RAPTOR_RAMFS_H */
