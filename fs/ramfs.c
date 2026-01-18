/* fs/ramfs.c - Complete RAM filesystem with file support
 *
 * COMPLETE IMPLEMENTATION:
 * - Directory creation/deletion ✓
 * - File creation/deletion ✓
 * - File read/write ✓
 * - Directory listing ✓
 *
 * Files are stored entirely in kernel heap memory.
 */

#include "ramfs.h"
#include "vfs.h"
#include "../kernel/kernel.h"
#include "../lib/string.h"

/* ====================================================================
 * RAMFS INTERNAL STRUCTURES
 * ==================================================================== */

/* Directory: contains list of children */
typedef struct ramfs_dir {
    vfs_node_t *children;  /* Linked list of child nodes */
} ramfs_dir_t;

/* File: contains data buffer */
typedef struct ramfs_file {
    uint8_t *data;        /* File contents */
    uint32_t size;        /* Current size */
    uint32_t capacity;    /* Allocated capacity */
} ramfs_file_t;

/* ====================================================================
 * FORWARD DECLARATIONS
 * ==================================================================== */

static int ramfs_open(vfs_node_t *node, uint32_t flags);
static int ramfs_close(vfs_node_t *node);
static int ramfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static int ramfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer);
static dirent_t *ramfs_readdir(vfs_node_t *node, uint32_t index);
static vfs_node_t *ramfs_finddir(vfs_node_t *node, const char *name);
static vfs_node_t *ramfs_create(vfs_node_t *parent, const char *name, uint32_t mode);
static int ramfs_unlink(vfs_node_t *parent, const char *name);
static vfs_node_t *ramfs_mkdir(vfs_node_t *parent, const char *name, uint32_t mode);
static int ramfs_rmdir(vfs_node_t *parent, const char *name);

/* ====================================================================
 * OPERATIONS TABLE
 * ==================================================================== */

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

/* ====================================================================
 * HELPER FUNCTIONS
 * ==================================================================== */

/* Allocate and initialize a vfs_node */
static vfs_node_t *ramfs_alloc_node(const char *name, vfs_node_type_t type, uint32_t mode)
{
    vfs_node_t *node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
    if (!node) return NULL;

    memset(node, 0, sizeof(vfs_node_t));
    
    if (name && name[0]) {
        strncpy(node->name, name, sizeof(node->name) - 1);
        node->name[sizeof(node->name) - 1] = '\0';
    } else {
        node->name[0] = '\0';
    }

    node->type = type;
    node->mode = mode;
    node->inode = (uint32_t)node;  /* Use address as inode */
    node->size = 0;
    node->open_count = 0;
    node->ops = &ramfs_ops;
    node->impl_data = NULL;
    node->parent = NULL;
    node->next = NULL;
    node->mounted = NULL;

    return node;
}

/* Attach child to parent directory */
static void ramfs_attach_child(vfs_node_t *parent, vfs_node_t *child)
{
    if (!parent || !child) return;

    /* Get or create directory structure */
    ramfs_dir_t *dir = (ramfs_dir_t *)parent->impl_data;
    if (!dir) {
        dir = (ramfs_dir_t *)kmalloc(sizeof(ramfs_dir_t));
        if (!dir) return;
        memset(dir, 0, sizeof(ramfs_dir_t));
        parent->impl_data = dir;
    }

    /* Add to head of children list */
    child->next = dir->children;
    dir->children = child;
    child->parent = parent;
}

/* Detach child from parent directory */
static vfs_node_t *ramfs_detach_child(vfs_node_t *parent, const char *name)
{
    if (!parent || !parent->impl_data) return NULL;
    
    ramfs_dir_t *dir = (ramfs_dir_t *)parent->impl_data;
    vfs_node_t **prev = &dir->children;

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

/* ====================================================================
 * FILE OPERATIONS
 * ==================================================================== */

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
    
    if (node->open_count > 0) {
        node->open_count--;
    }
    
    return 0;
}

static int ramfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer)
{
    if (!node || node->type != VFS_FILE || !buffer) {
        return -1;
    }

    ramfs_file_t *file = (ramfs_file_t *)node->impl_data;
    if (!file || !file->data) {
        return 0;  /* Empty file */
    }

    /* Don't read past end of file */
    if (offset >= file->size) {
        return 0;  /* EOF */
    }

    uint32_t bytes_to_read = size;
    if (offset + bytes_to_read > file->size) {
        bytes_to_read = file->size - offset;
    }

    /* Copy data to buffer */
    memcpy(buffer, file->data + offset, bytes_to_read);
    
    return bytes_to_read;
}

static int ramfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer)
{
    if (!node || node->type != VFS_FILE || !buffer) {
        return -1;
    }

    /* Get or create file structure */
    ramfs_file_t *file = (ramfs_file_t *)node->impl_data;
    if (!file) {
        file = (ramfs_file_t *)kmalloc(sizeof(ramfs_file_t));
        if (!file) return -1;
        
        file->data = NULL;
        file->size = 0;
        file->capacity = 0;
        node->impl_data = file;
    }

    /* Calculate required capacity */
    uint32_t required_size = offset + size;
    
    /* Resize buffer if needed */
    if (required_size > file->capacity) {
        /* Allocate new capacity (round up to nearest 256 bytes) */
        uint32_t new_capacity = (required_size + 255) & ~255;
        
        uint8_t *new_data = (uint8_t *)kmalloc(new_capacity);
        if (!new_data) return -1;
        
        /* Copy existing data if any */
        if (file->data && file->size > 0) {
            memcpy(new_data, file->data, file->size);
        }
        
        /* Free old buffer */
        if (file->data) {
            kfree(file->data);
        }
        
        file->data = new_data;
        file->capacity = new_capacity;
    }

    /* Write data */
    memcpy(file->data + offset, buffer, size);
    
    /* Update size if we wrote past end */
    if (required_size > file->size) {
        file->size = required_size;
        node->size = required_size;
    }

    return size;
}

/* ====================================================================
 * DIRECTORY OPERATIONS
 * ==================================================================== */

static dirent_t *ramfs_readdir(vfs_node_t *node, uint32_t index)
{
    if (!node || node->type != VFS_DIRECTORY) {
        return NULL;
    }

    static dirent_t dent;  /* Static OK for single-threaded kernel */
    
    ramfs_dir_t *dir = (ramfs_dir_t *)node->impl_data;
    if (!dir) return NULL;

    /* Walk to the requested index */
    vfs_node_t *child = dir->children;
    uint32_t i = 0;
    
    while (child && i < index) {
        child = child->next;
        i++;
    }

    if (!child) return NULL;

    /* Fill in directory entry */
    strncpy(dent.name, child->name, sizeof(dent.name) - 1);
    dent.name[sizeof(dent.name) - 1] = '\0';
    dent.inode = child->inode;
    dent.type = (uint32_t)child->type;

    return &dent;
}

static vfs_node_t *ramfs_finddir(vfs_node_t *node, const char *name)
{
    if (!node || node->type != VFS_DIRECTORY) {
        return NULL;
    }

    ramfs_dir_t *dir = (ramfs_dir_t *)node->impl_data;
    if (!dir) return NULL;

    /* Search children */
    for (vfs_node_t *child = dir->children; child; child = child->next) {
        if (strcmp(child->name, name) == 0) {
            return child;
        }
    }

    return NULL;
}

static vfs_node_t *ramfs_create(vfs_node_t *parent, const char *name, uint32_t mode)
{
    if (!parent || parent->type != VFS_DIRECTORY) {
        return NULL;
    }

    /* Check if already exists */
    if (ramfs_finddir(parent, name)) {
        return NULL;
    }

    /* Create new file node */
    vfs_node_t *node = ramfs_alloc_node(name, VFS_FILE, mode);
    if (!node) return NULL;

    /* Create file structure */
    ramfs_file_t *file = (ramfs_file_t *)kmalloc(sizeof(ramfs_file_t));
    if (!file) {
        kfree(node);
        return NULL;
    }
    
    file->data = NULL;
    file->size = 0;
    file->capacity = 0;
    node->impl_data = file;

    /* Attach to parent */
    ramfs_attach_child(parent, node);

    return node;
}

static int ramfs_unlink(vfs_node_t *parent, const char *name)
{
    if (!parent || parent->type != VFS_DIRECTORY) {
        return -1;
    }

    /* Detach from parent */
    vfs_node_t *victim = ramfs_detach_child(parent, name);
    if (!victim) return -1;

    /* Don't allow unlinking directories */
    if (victim->type == VFS_DIRECTORY) {
        /* Put it back */
        ramfs_attach_child(parent, victim);
        return -1;
    }

    /* Free file data if it exists */
    if (victim->impl_data) {
        ramfs_file_t *file = (ramfs_file_t *)victim->impl_data;
        if (file->data) {
            kfree(file->data);
        }
        kfree(file);
    }

    /* Free the node itself */
    kfree(victim);

    return 0;
}

static vfs_node_t *ramfs_mkdir(vfs_node_t *parent, const char *name, uint32_t mode)
{
    if (!parent || parent->type != VFS_DIRECTORY) {
        return NULL;
    }

    /* Check if already exists */
    if (ramfs_finddir(parent, name)) {
        return NULL;
    }

    /* Create new directory node */
    vfs_node_t *node = ramfs_alloc_node(name, VFS_DIRECTORY, mode);
    if (!node) return NULL;

    /* Create directory structure */
    ramfs_dir_t *dir = (ramfs_dir_t *)kmalloc(sizeof(ramfs_dir_t));
    if (!dir) {
        kfree(node);
        return NULL;
    }
    
    memset(dir, 0, sizeof(ramfs_dir_t));
    node->impl_data = dir;

    /* Attach to parent */
    ramfs_attach_child(parent, node);

    return node;
}

static int ramfs_rmdir(vfs_node_t *parent, const char *name)
{
    if (!parent || parent->type != VFS_DIRECTORY) {
        return -1;
    }

    /* Find and detach */
    vfs_node_t *victim = ramfs_detach_child(parent, name);
    if (!victim) return -1;

    /* Must be a directory */
    if (victim->type != VFS_DIRECTORY) {
        ramfs_attach_child(parent, victim);
        return -1;
    }

    /* Must be empty */
    ramfs_dir_t *dir = (ramfs_dir_t *)victim->impl_data;
    if (dir && dir->children) {
        ramfs_attach_child(parent, victim);
        return -1;  /* Directory not empty */
    }

    /* Free directory structure */
    if (dir) {
        kfree(dir);
    }

    /* Free the node */
    kfree(victim);

    return 0;
}

/* ====================================================================
 * INITIALIZATION
 * ==================================================================== */

void ramfs_init(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("[RAMFS] Creating RAM filesystem...\n");

    /* Create root directory */
    vfs_node_t *root = ramfs_alloc_node("", VFS_DIRECTORY, S_IRWXU | S_IRWXG | S_IRWXO);
    if (!root) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("[RAMFS] ERROR: Failed to create root directory\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        return;
    }

    /* Create directory structure for root */
    ramfs_dir_t *rootdir = (ramfs_dir_t *)kmalloc(sizeof(ramfs_dir_t));
    if (!rootdir) {
        kfree(root);
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("[RAMFS] ERROR: Failed to allocate root directory structure\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        return;
    }
    
    memset(rootdir, 0, sizeof(ramfs_dir_t));
    root->impl_data = rootdir;

    /* Set as VFS root and current directory */
    vfs_root = root;
    vfs_cwd = root;

    /* Create standard directories */
    ramfs_mkdir(root, "bin", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    ramfs_mkdir(root, "dev", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    ramfs_mkdir(root, "tmp", S_IRWXU | S_IRWXG | S_IRWXO);
    ramfs_mkdir(root, "home", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[RAMFS] RAM filesystem created successfully\n");
    terminal_writestring("[RAMFS] Created: /bin, /dev, /tmp, /home\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}