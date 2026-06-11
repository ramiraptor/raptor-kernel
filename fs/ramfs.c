// SPDX-License-Identifier: GPL-2.0-only
/*
 * ramfs.c - hierarchical in-memory filesystem.
 *
 * The whole tree lives on the kernel heap and vanishes on reboot,
 * exactly like Linux's ramfs. There are no inodes, permissions or
 * timestamps yet - just names, directories and byte streams - but the
 * path resolution semantics ("/", ".", "..", relative lookups) match
 * what every Unix user expects, which is what makes the shell feel
 * like home.
 */

#include <raptor/mm.h>
#include <raptor/ramfs.h>
#include <raptor/string.h>
#include <raptor/version.h>

static struct ramfs_node *root;

struct ramfs_node *ramfs_root(void)
{
    return root;
}

static struct ramfs_node *node_new(const char *name, enum ramfs_type type)
{
    struct ramfs_node *n = kzalloc(sizeof(*n));

    if (!n)
        return NULL;
    strlcpy(n->name, name, sizeof(n->name));
    n->type = type;
    return n;
}

static void attach(struct ramfs_node *parent, struct ramfs_node *child)
{
    child->parent = parent;
    child->next = parent->children;
    parent->children = child;
}

static void detach(struct ramfs_node *node)
{
    struct ramfs_node **pp = &node->parent->children;

    while (*pp && *pp != node)
        pp = &(*pp)->next;
    if (*pp)
        *pp = node->next;
    node->parent = NULL;
    node->next = NULL;
}

static struct ramfs_node *find_child(const struct ramfs_node *dir,
                                     const char *name)
{
    for (struct ramfs_node *c = dir->children; c; c = c->next) {
        if (strcmp(c->name, name) == 0)
            return c;
    }
    return NULL;
}

/*
 * Walk a path starting from `cwd` (or the root for absolute paths).
 * If `parent_out` is non-NULL, resolution stops at the last component:
 * the containing directory is returned through `parent_out`, the final
 * name through `name_out`, and the node itself (which may not exist)
 * as the return value.
 */
static struct ramfs_node *walk(struct ramfs_node *cwd, const char *path,
                               struct ramfs_node **parent_out,
                               char *name_out)
{
    struct ramfs_node *cur = (path[0] == '/') ? root : cwd;
    char component[RAMFS_NAME_MAX];
    const char *p = path;

    if (parent_out) {
        *parent_out = NULL;
        name_out[0] = '\0';
    }

    for (;;) {
        while (*p == '/')
            p++;
        if (!*p)
            break;

        size_t len = 0;
        while (p[len] && p[len] != '/')
            len++;
        if (len >= sizeof(component))
            return NULL;                     /* component too long */
        memcpy(component, p, len);
        component[len] = '\0';

        const char *rest = p + len;
        while (*rest == '/')
            rest++;
        bool last = (*rest == '\0');

        struct ramfs_node *next;
        if (strcmp(component, ".") == 0) {
            next = cur;
        } else if (strcmp(component, "..") == 0) {
            next = cur->parent ? cur->parent : cur;
        } else {
            if (cur->type != RAMFS_DIR)
                return NULL;
            next = find_child(cur, component);
        }

        if (last && parent_out && strcmp(component, ".") != 0 &&
            strcmp(component, "..") != 0) {
            *parent_out = cur;
            strlcpy(name_out, component, RAMFS_NAME_MAX);
            return next;                     /* may be NULL: not created yet */
        }
        if (!next)
            return NULL;

        cur = next;
        p = rest;
    }
    return cur;
}

struct ramfs_node *ramfs_resolve(struct ramfs_node *cwd, const char *path)
{
    return walk(cwd ? cwd : root, path, NULL, NULL);
}

struct ramfs_node *ramfs_create(struct ramfs_node *cwd, const char *path,
                                enum ramfs_type type)
{
    struct ramfs_node *parent;
    char name[RAMFS_NAME_MAX];

    struct ramfs_node *existing = walk(cwd ? cwd : root, path, &parent, name);

    if (existing || !parent || !name[0] || parent->type != RAMFS_DIR)
        return NULL;

    struct ramfs_node *n = node_new(name, type);
    if (n)
        attach(parent, n);
    return n;
}

bool ramfs_write(struct ramfs_node *file, const char *data, size_t len,
                 bool append)
{
    if (!file || file->type != RAMFS_FILE)
        return false;

    size_t base = append ? file->size : 0;
    size_t needed = base + len;

    if (needed > file->capacity) {
        size_t newcap = file->capacity ? file->capacity : 64;
        while (newcap < needed)
            newcap *= 2;

        char *buf = kmalloc(newcap);
        if (!buf)
            return false;
        if (base)
            memcpy(buf, file->data, base);
        kfree(file->data);
        file->data = buf;
        file->capacity = newcap;
    }

    memcpy(file->data + base, data, len);
    file->size = base + len;
    return true;
}

static void free_tree(struct ramfs_node *node)
{
    struct ramfs_node *c = node->children;

    while (c) {
        struct ramfs_node *next = c->next;
        free_tree(c);
        c = next;
    }
    kfree(node->data);
    kfree(node);
}

bool ramfs_delete(struct ramfs_node *node, bool recursive)
{
    if (!node || node == root || !node->parent)
        return false;
    if (node->type == RAMFS_DIR && node->children && !recursive)
        return false;

    detach(node);
    free_tree(node);
    return true;
}

bool ramfs_move(struct ramfs_node *node, struct ramfs_node *new_parent,
                const char *new_name)
{
    if (!node || node == root || !new_parent ||
        new_parent->type != RAMFS_DIR)
        return false;
    if (find_child(new_parent, new_name))
        return false;                        /* destination exists */

    /* Refuse to move a directory into its own subtree. */
    for (const struct ramfs_node *p = new_parent; p; p = p->parent) {
        if (p == node)
            return false;
    }

    detach(node);
    strlcpy(node->name, new_name, sizeof(node->name));
    attach(new_parent, node);
    return true;
}

void ramfs_path(const struct ramfs_node *node, char *buf, size_t len)
{
    if (!node->parent) {                     /* the root */
        strlcpy(buf, "/", len);
        return;
    }

    /* Collect ancestors, then print them top-down. */
    const struct ramfs_node *chain[32];
    int depth = 0;

    for (const struct ramfs_node *n = node; n->parent && depth < 32;
         n = n->parent)
        chain[depth++] = n;

    buf[0] = '\0';
    for (int i = depth - 1; i >= 0; i--) {
        strlcpy(buf + strlen(buf), "/", len - strlen(buf));
        strlcpy(buf + strlen(buf), chain[i]->name, len - strlen(buf));
    }
}

/* ---- initial tree ---------------------------------------------------- */

static void preload_file(const char *path, const char *contents)
{
    struct ramfs_node *f = ramfs_create(root, path, RAMFS_FILE);

    if (f)
        ramfs_write(f, contents, strlen(contents), false);
}

void ramfs_init(void)
{
    root = node_new("", RAMFS_DIR);

    const char *dirs[] = { "/bin", "/etc", "/home", "/home/user",
                           "/tmp", "/usr", "/var" };
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++)
        ramfs_create(root, dirs[i], RAMFS_DIR);

    preload_file("/etc/motd",
        "Welcome to " RAPTOR_NAME " " RAPTOR_VERSION
        " (" RAPTOR_CODENAME ")!\n"
        "Type 'help' to see the available commands.\n");

    preload_file("/etc/os-release",
        "NAME=\"" RAPTOR_NAME "\"\n"
        "VERSION=\"" RAPTOR_VERSION " (" RAPTOR_CODENAME ")\"\n"
        "ID=raptor\n"
        "PRETTY_NAME=\"" RAPTOR_NAME " " RAPTOR_VERSION "\"\n"
        "HOME_URL=\"https://github.com/raptor-kernel\"\n");

    preload_file("/README.txt",
        RAPTOR_NAME " is a small Unix-like teaching kernel.\n"
        "\n"
        "Everything you see in this filesystem lives in RAM and is\n"
        "rebuilt on every boot. Try things like:\n"
        "\n"
        "    ls /etc\n"
        "    cat /etc/motd\n"
        "    echo hello > /tmp/note && cat /tmp/note\n"
        "    free\n"
        "    uname -a\n");
}
