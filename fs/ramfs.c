/* fs/ramfs.c - Simple in-memory RAM filesystem (directory-only first pass)
 *
 * This implementation is intentionally small and self-contained so it
 * cleanly fits your VFS interface. It implements directory creation,
 * lookup and readdir, mounts a root filesystem and creates /bin, /dev, /tmp.
 *
 * Later we can add file data buffers, read/write, unlink, permissions, etc.
 */

#include "ramfs.h"
#include "vfs.h"
#include <stdint.h>
#include <stddef.h>
#include "../mm/heap.h" /* kmalloc/kfree */
#include "../lib/string.h"     /* strncpy/strcmp/memset - adapt if paths differ */
/* -----------------------------
 * Internal RAMFS struct`ures
 * ----------------------------- */

/* For directories we keep a simple singly-linked list of child vfs_node_t.
 * The head pointer for a directory is stored inside impl_data as a ramfs_dir_t.
 */

typedef struct ramfs_dir {
    vfs_node_t *children; /* head of linked list of children */
} ramfs_dir_t;

/* For files (future) we may use this structure and store pointer in impl_data */
typedef struct ramfs_file {
    uint8_t *data;
    uint32_t size;
    uint32_t capacity;
} ramfs_file_t;

/* Forward declarations of operations */
static int  ramfs_open(vfs_node_t *node, uint32_t flags);
static int  ramfs_close(vfs_node_t *node);
static int  ramfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static int  ramfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer);
static dirent_t *ramfs_readdir(vfs_node_t *node, uint32_t index);
static vfs_node_t *ramfs_finddir(vfs_node_t *node, const char *name);
static vfs_node_t *ramfs_create(vfs_node_t *parent, const char *name, uint32_t mode);
static int ramfs_unlink(vfs_node_t *parent, const char *name);
static vfs_node_t *ramfs_mkdir(vfs_node_t *parent, const char *name, uint32_t mode);
static int ramfs_rmdir(vfs_node_t *parent, const char *name);

/* Operations table for RAMFS */
static vfs_operations_t ramfs_ops = {
    .open    = ramfs_open,
    .close   = ramfs_close,
    .read    = ramfs_read,
    .write   = ramfs_write,
    .readdir = ramfs_readdir,
    .finddir = ramfs_finddir,
    .create  = ramfs_create,
    .unlink  = ramfs_unlink,
    .mkdir   = ramfs_mkdir,
    .rmdir   = ramfs_rmdir,
};

/* Helper: allocate and zero a vfs_node */
static vfs_node_t *ramfs_alloc_vfs_node(const char *name, vfs_node_type_t type, uint32_t mode)
{
    vfs_node_t *node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
    if (!node) return NULL;

    memset(node, 0, sizeof(vfs_node_t));
    if (name && name[0])
        strncpy(node->name, name, sizeof(node->name) - 1);
    else
        node->name[0] = '\0';

    node->type = type;
    node->mode = mode;
    node->inode = (uint32_t)node; /* cheap unique inode */
    node->size = 0;
    node->open_count = 0;
    node->ops = &ramfs_ops;
    node->impl_data = NULL;
    node->parent = NULL;
    node->next = NULL;

    return node;
}

/* Helper: attach child to parent directory (head insertion) */
static void ramfs_attach_child(vfs_node_t *parent, vfs_node_t *child)
{
    if (!parent || !child) return;

    ramfs_dir_t *pdir = (ramfs_dir_t *)parent->impl_data;
    if (!pdir) {
        pdir = (ramfs_dir_t *)kmalloc(sizeof(ramfs_dir_t));
        memset(pdir, 0, sizeof(ramfs_dir_t));
        parent->impl_data = (void *)pdir;
    }

    /* Insert at head */
    child->next = pdir->children;
    pdir->children = child;
    child->parent = parent;
}

/* Helper: detach child by name from parent. Returns pointer to removed node or NULL */
static vfs_node_t *ramfs_detach_child(vfs_node_t *parent, const char *name)
{
    if (!parent || !parent->impl_data) return NULL;
    ramfs_dir_t *pdir = (ramfs_dir_t *)parent->impl_data;
    vfs_node_t **prev = &pdir->children;

    while (*prev) {
        if (strcmp((*prev)->name, name) == 0) {
            vfs_node_t *victim = *prev;
            *prev = victim->next;
            victim->next = NULL;
            victim->parent = NULL;
            return victim;
        }
        prev = &(*prev)->next;
    }
    return NULL;
}

/* -----------------------------
 * Operations (minimal, directory-focused)
 * ----------------------------- */

static int ramfs_open(vfs_node_t *node, uint32_t flags)
{
    (void)flags;
    if (!node) return -1;
    node->open_count++;
    return 0;
}

static int ramfs_close(vfs_node_t *node)
{
    if (!node) return -1;
    if (node->open_count > 0) node->open_count--;
    return 0;
}

/* read/write: not implemented yet (return -1) */
static int ramfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer)
{
    (void)node; (void)offset; (void)size; (void)buffer;
    return -1;
}

static int ramfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer)
{
    (void)node; (void)offset; (void)size; (void)buffer;
    return -1;
}

static dirent_t *ramfs_readdir(vfs_node_t *node, uint32_t index)
{
    if (!node || node->type != VFS_DIRECTORY) return NULL;

    static dirent_t d; /* single static dirent — single-threaded kernel OK */
    ramfs_dir_t *pdir = (ramfs_dir_t *)node->impl_data;
    if (!pdir) return NULL;

    vfs_node_t *child = pdir->children;
    uint32_t i = 0;
    while (child && i < index) {
        child = child->next;
        i++;
    }

    if (!child) return NULL;

    strncpy(d.name, child->name, sizeof(d.name) - 1);
    d.name[sizeof(d.name) - 1] = '\0';
    d.inode = child->inode;
    d.type = (uint32_t)child->type;
    return &d;
}

static vfs_node_t *ramfs_finddir(vfs_node_t *node, const char *name)
{
    if (!node || node->type != VFS_DIRECTORY) return NULL;
    ramfs_dir_t *pdir = (ramfs_dir_t *)node->impl_data;
    if (!pdir) return NULL;

    for (vfs_node_t *c = pdir->children; c; c = c->next) {
        if (strcmp(c->name, name) == 0)
            return c;
    }
    return NULL;
}

static vfs_node_t *ramfs_create(vfs_node_t *parent, const char *name, uint32_t mode)
{
    if (!parent || parent->type != VFS_DIRECTORY) return NULL;
    if (ramfs_finddir(parent, name)) return NULL; /* exists */

    vfs_node_t *node = ramfs_alloc_vfs_node(name, VFS_FILE, mode);
    if (!node) return NULL;

    /* For file nodes we can initialize file-specific impl_data later */
    ramfs_attach_child(parent, node);
    return node;
}

static int ramfs_unlink(vfs_node_t *parent, const char *name)
{
    if (!parent || parent->type != VFS_DIRECTORY) return -1;
    vfs_node_t *victim = ramfs_detach_child(parent, name);
    if (!victim) return -1;

    /* If it had children, refuse (should be directory rmdir) */
    if (victim->type == VFS_DIRECTORY) {
        ramfs_dir_t *d = (ramfs_dir_t *)victim->impl_data;
        if (d && d->children) {
            /* put it back */
            ramfs_attach_child(parent, victim);
            return -1; /* directory not empty */
        }
    }

    /* Free impl_data if any */
    if (victim->impl_data) kfree(victim->impl_data);
    kfree(victim);
    return 0;
}

static vfs_node_t *ramfs_mkdir(vfs_node_t *parent, const char *name, uint32_t mode)
{
    if (!parent || parent->type != VFS_DIRECTORY) return NULL;
    if (ramfs_finddir(parent, name)) return NULL; /* already exists */

    vfs_node_t *node = ramfs_alloc_vfs_node(name, VFS_DIRECTORY, mode);
    if (!node) return NULL;

    /* allocate an empty dir header */
    ramfs_dir_t *d = (ramfs_dir_t *)kmalloc(sizeof(ramfs_dir_t));
    if (!d) {
        kfree(node);
        return NULL;
    }
    memset(d, 0, sizeof(ramfs_dir_t));
    node->impl_data = d;

    ramfs_attach_child(parent, node);
    return node;
}

static int ramfs_rmdir(vfs_node_t *parent, const char *name)
{
    if (!parent || parent->type != VFS_DIRECTORY) return -1;

    vfs_node_t *victim = ramfs_detach_child(parent, name);
    if (!victim) return -1;

    if (victim->type != VFS_DIRECTORY) {
        /* Not a directory */
        ramfs_attach_child(parent, victim);
        return -1;
    }

    ramfs_dir_t *d = (ramfs_dir_t *)victim->impl_data;
    if (d && d->children) {
        /* Not empty: restore and fail */
        ramfs_attach_child(parent, victim);
        return -1;
    }

    if (victim->impl_data) kfree(victim->impl_data);
    kfree(victim);
    return 0;
}

/* -----------------------------
 * Initialization: create root and common directories
 * ----------------------------- */

void ramfs_init(void)
{
    /* Create root node (name "") */
    vfs_node_t *root = ramfs_alloc_vfs_node("", VFS_DIRECTORY,
                                            S_IRWXU | S_IRWXG | S_IRWXO);
    if (!root) return;

    /* Give root an empty directory header */
    ramfs_dir_t *rootdir = (ramfs_dir_t *)kmalloc(sizeof(ramfs_dir_t));
    if (!rootdir) {
        kfree(root);
        return;
    }
    memset(rootdir, 0, sizeof(ramfs_dir_t));
    root->impl_data = rootdir;

    /* Set global VFS root & cwd */
    vfs_root = root;
    vfs_cwd = root;

    /* Create /bin, /dev, /tmp */
    ramfs_mkdir(root, "bin", S_IRWXU);
    ramfs_mkdir(root, "dev", S_IRWXU);
    ramfs_mkdir(root, "tmp", S_IRWXU | S_IRWXG | S_IRWXO);
}

