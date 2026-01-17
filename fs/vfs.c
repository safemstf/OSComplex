/* fs/vfs.c - Virtual File System Implementation
 *
 * This implements the core VFS layer that sits between user code
 * and actual filesystems (ramfs, tarfs, etc.)
 *
 * KEY RESPONSIBILITIES:
 * 1. Path resolution ("/path/to/file" → vfs_node)
 * 2. File descriptor management
 * 3. Mount point handling
 * 4. Delegating to filesystem-specific operations
 */

#include "vfs.h"
#include "../kernel/kernel.h"
#include "../lib/string.h"

/* ====================================================================
 * GLOBAL STATE
 * ==================================================================== */

/* Root filesystem node (/) - initialized by vfs_init() */
vfs_node_t *vfs_root = NULL;

/* Current working directory - starts at root */
vfs_node_t *vfs_cwd = NULL;

/* File descriptor table - tracks open files
 * TODO: Per-process FD table when we add process management */
static file_descriptor_t fd_table[MAX_OPEN_FILES];

/* Mount points list */
static vfs_mount_t *mount_list = NULL;

/* ====================================================================
 * FILE DESCRIPTOR MANAGEMENT
 * ==================================================================== */

/* Allocate a new file descriptor
 * Returns FD number, or -1 if table is full */
static int fd_alloc(void)
{
    for (int i = 0; i < MAX_OPEN_FILES; i++)
    {
        if (!fd_table[i].in_use)
        {
            fd_table[i].in_use = true;
            fd_table[i].position = 0;
            return i;
        }
    }
    return -1; /* No free slots */
}

/* Free a file descriptor */
static void fd_free(int fd)
{
    if (fd >= 0 && fd < MAX_OPEN_FILES)
    {
        fd_table[fd].in_use = false;
        fd_table[fd].node = NULL;
        fd_table[fd].position = 0;
        fd_table[fd].flags = 0;
    }
}

/* Get file descriptor entry
 * Returns NULL if invalid FD */
static file_descriptor_t *fd_get(int fd)
{
    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].in_use)
    {
        return NULL;
    }
    return &fd_table[fd];
}

/* ====================================================================
 * PATH PARSING HELPERS
 * ==================================================================== */

/* Split path into components
 * "/home/user/file" → ["home", "user", "file"]
 * Returns number of components */
static int path_split(const char *path, char components[][256], int max_components)
{
    int count = 0;
    const char *p = path;

    /* Skip leading slashes */
    while (*p == '/')
        p++;

    while (*p && count < max_components)
    {
        /* Extract component */
        int i = 0;
        while (*p && *p != '/' && i < 255)
        {
            components[count][i++] = *p++;
        }
        components[count][i] = '\0';

        if (i > 0)
        {
            count++;
        }

        /* Skip slashes */
        while (*p == '/')
            p++;
    }

    return count;
}

/* ====================================================================
 * PATH RESOLUTION
 *
 * This is the heart of VFS - converts paths to vfs_nodes
 * ==================================================================== */

vfs_node_t *vfs_resolve_path(const char *path)
{
    if (!path || !*path)
    {
        return NULL;
    }

    /* Start from root or cwd depending on path */
    vfs_node_t *current;
    if (path[0] == '/')
    {
        current = vfs_root; /* Absolute path */
    }
    else
    {
        current = vfs_cwd; /* Relative path */
    }

    if (!current)
    {
        return NULL;
    }

    /* Handle special cases */
    if (strcmp(path, "/") == 0)
    {
        return vfs_root;
    }
    if (strcmp(path, ".") == 0)
    {
        return vfs_cwd;
    }
    if (strcmp(path, "..") == 0 && vfs_cwd && vfs_cwd->parent)
    {
        return vfs_cwd->parent;
    }

    /* Split path into components */
    char components[32][256];
    int num_components = path_split(path, components, 32);

    /* Walk the path */
    for (int i = 0; i < num_components; i++)
    {
        /* Handle "." and ".." */
        if (strcmp(components[i], ".") == 0)
        {
            continue; /* Stay in current directory */
        }
        if (strcmp(components[i], "..") == 0)
        {
            if (current->parent)
            {
                current = current->parent;
            }
            continue;
        }

        /* Check if current is a directory */
        if (current->type != VFS_DIRECTORY && current->type != VFS_MOUNTPOINT)
        {
            return NULL; /* Can't traverse through non-directory */
        }

        /* Check for mount point */
        if (current->mounted)
        {
            current = current->mounted;
        }

        /* Find child with this name */
        if (!current->ops || !current->ops->finddir)
        {
            return NULL; /* No finddir operation */
        }

        vfs_node_t *child = current->ops->finddir(current, components[i]);
        if (!child)
        {
            return NULL; /* Component not found */
        }

        current = child;
    }

    /* Check for mount point at final node */
    if (current->mounted)
    {
        current = current->mounted;
    }

    return current;
}

/* ====================================================================
 * FILE OPERATIONS
 * ==================================================================== */

int vfs_open(const char *path, uint32_t flags)
{
    /* Resolve path to node */
    vfs_node_t *node = vfs_resolve_path(path);

    /* If file doesn't exist and O_CREAT is set, try to create it */
    if (!node && (flags & O_CREAT))
    {
        /* Extract parent directory and filename */
        char path_copy[256];
        strncpy(path_copy, path, 255);
        path_copy[255] = '\0';

        /* Find last slash */
        char *last_slash = strrchr(path_copy, '/');
        if (!last_slash)
        {
            return -1; /* Invalid path */
        }

        *last_slash = '\0'; /* Split into parent and name */
        const char *parent_path = path_copy;
        const char *filename = last_slash + 1;

        /* Handle root directory case */
        if (parent_path[0] == '\0')
        {
            parent_path = "/";
        }

        /* Find parent directory */
        vfs_node_t *parent = vfs_resolve_path(parent_path);
        if (!parent || parent->type != VFS_DIRECTORY)
        {
            return -1; /* Parent not found or not a directory */
        }

        /* Create the file */
        if (!parent->ops || !parent->ops->create)
        {
            return -1; /* Filesystem doesn't support create */
        }

        node = parent->ops->create(parent, filename, S_IRUSR | S_IWUSR);
        if (!node)
        {
            return -1; /* Create failed */
        }
    }

    if (!node)
    {
        return -1; /* File not found */
    }

    /* Allocate file descriptor */
    int fd = fd_alloc();
    if (fd < 0)
    {
        return -1; /* No free file descriptors */
    }

    /* Call filesystem-specific open if it exists */
    if (node->ops && node->ops->open)
    {
        if (node->ops->open(node, flags) < 0)
        {
            fd_free(fd);
            return -1;
        }
    }

    /* Set up file descriptor */
    fd_table[fd].node = node;
    fd_table[fd].flags = flags;
    fd_table[fd].position = 0;

    /* If truncate flag, set size to 0 */
    if (flags & O_TRUNC)
    {
        node->size = 0;
    }

    /* If append flag, start at end */
    if (flags & O_APPEND)
    {
        fd_table[fd].position = node->size;
    }

    /* Increment reference count */
    node->open_count++;

    return fd;
}

int vfs_close(int fd)
{
    file_descriptor_t *file = fd_get(fd);
    if (!file)
    {
        return -1; /* Invalid FD */
    }

    vfs_node_t *node = file->node;

    /* Call filesystem-specific close if it exists */
    if (node->ops && node->ops->close)
    {
        node->ops->close(node);
    }

    /* Decrement reference count */
    if (node->open_count > 0)
    {
        node->open_count--;
    }

    /* Free the file descriptor */
    fd_free(fd);

    return 0;
}

int vfs_read(int fd, void *buffer, uint32_t size)
{
    file_descriptor_t *file = fd_get(fd);
    if (!file)
    {
        return -1; /* Invalid FD */
    }

    /* Check if opened for reading */
    if ((file->flags & O_WRONLY) && !(file->flags & O_RDWR))
    {
        return -1; /* Write-only file */
    }

    vfs_node_t *node = file->node;

    /* Call filesystem-specific read */
    if (!node->ops || !node->ops->read)
    {
        return -1; /* No read operation */
    }

    int bytes_read = node->ops->read(node, file->position, size, (uint8_t *)buffer);

    if (bytes_read > 0)
    {
        file->position += bytes_read;
    }

    return bytes_read;
}

int vfs_write(int fd, const void *buffer, uint32_t size)
{
    file_descriptor_t *file = fd_get(fd);
    if (!file)
    {
        return -1; /* Invalid FD */
    }

    /* Check if opened for writing */
    if ((file->flags & O_RDONLY) && !(file->flags & O_RDWR))
    {
        return -1; /* Read-only file */
    }

    vfs_node_t *node = file->node;

    /* Call filesystem-specific write */
    if (!node->ops || !node->ops->write)
    {
        return -1; /* No write operation */
    }

    int bytes_written = node->ops->write(node, file->position, size, (const uint8_t *)buffer);

    if (bytes_written > 0)
    {
        file->position += bytes_written;

        /* Update file size if we wrote past end */
        if (file->position > node->size)
        {
            node->size = file->position;
        }
    }

    return bytes_written;
}

int vfs_seek(int fd, int32_t offset, int whence)
{
    file_descriptor_t *file = fd_get(fd);
    if (!file)
    {
        return -1; /* Invalid FD */
    }

    vfs_node_t *node = file->node;
    int32_t new_pos = 0;

    switch (whence)
    {
    case SEEK_SET:
        new_pos = offset;
        break;
    case SEEK_CUR:
        new_pos = file->position + offset;
        break;
    case SEEK_END:
        new_pos = node->size + offset;
        break;
    default:
        return -1; /* Invalid whence */
    }

    /* Don't allow seeking before start */
    if (new_pos < 0)
    {
        new_pos = 0;
    }

    file->position = new_pos;
    return new_pos;
}

dirent_t *vfs_readdir(int fd, uint32_t index)
{
    file_descriptor_t *file = fd_get(fd);
    if (!file)
    {
        return NULL; /* Invalid FD */
    }

    vfs_node_t *node = file->node;

    /* Must be a directory */
    if (node->type != VFS_DIRECTORY)
    {
        return NULL;
    }

    /* Call filesystem-specific readdir */
    if (!node->ops || !node->ops->readdir)
    {
        return NULL;
    }

    return node->ops->readdir(node, index);
}

/* ====================================================================
 * DIRECTORY OPERATIONS
 * ==================================================================== */

int vfs_mkdir(const char *path, uint32_t mode)
{
    /* Extract parent directory and directory name */
    char path_copy[256];
    strncpy(path_copy, path, 255);
    path_copy[255] = '\0';

    char *last_slash = strrchr(path_copy, '/');
    if (!last_slash)
    {
        return -1; /* Invalid path */
    }

    *last_slash = '\0';
    const char *parent_path = path_copy;
    const char *dirname = last_slash + 1;

    if (parent_path[0] == '\0')
    {
        parent_path = "/";
    }

    /* Find parent directory */
    vfs_node_t *parent = vfs_resolve_path(parent_path);
    if (!parent || parent->type != VFS_DIRECTORY)
    {
        return -1;
    }

    /* Call filesystem-specific mkdir */
    if (!parent->ops || !parent->ops->mkdir)
    {
        return -1;
    }

    vfs_node_t *new_dir = parent->ops->mkdir(parent, dirname, mode);
    return new_dir ? 0 : -1;
}

int vfs_rmdir(const char *path)
{
    char path_copy[256];
    strncpy(path_copy, path, 255);
    path_copy[255] = '\0';

    char *last_slash = strrchr(path_copy, '/');
    if (!last_slash)
    {
        return -1;
    }

    *last_slash = '\0';
    const char *parent_path = path_copy;
    const char *dirname = last_slash + 1;

    if (parent_path[0] == '\0')
    {
        parent_path = "/";
    }

    vfs_node_t *parent = vfs_resolve_path(parent_path);
    if (!parent || parent->type != VFS_DIRECTORY)
    {
        return -1;
    }

    if (!parent->ops || !parent->ops->rmdir)
    {
        return -1;
    }

    return parent->ops->rmdir(parent, dirname);
}

int vfs_unlink(const char *path)
{
    char path_copy[256];
    strncpy(path_copy, path, 255);
    path_copy[255] = '\0';

    char *last_slash = strrchr(path_copy, '/');
    if (!last_slash)
    {
        return -1;
    }

    *last_slash = '\0';
    const char *parent_path = path_copy;
    const char *filename = last_slash + 1;

    if (parent_path[0] == '\0')
    {
        parent_path = "/";
    }

    vfs_node_t *parent = vfs_resolve_path(parent_path);
    if (!parent || parent->type != VFS_DIRECTORY)
    {
        return -1;
    }

    if (!parent->ops || !parent->ops->unlink)
    {
        return -1;
    }

    return parent->ops->unlink(parent, filename);
}

/* ====================================================================
 * MOUNT OPERATIONS
 * ==================================================================== */

int vfs_mount(const char *device, const char *path, const char *fs_type, vfs_node_t *root)
{
    /* Find the mount point */
    vfs_node_t *mount_point = vfs_resolve_path(path);
    if (!mount_point)
    {
        return -1;
    }

    /* Must be a directory */
    if (mount_point->type != VFS_DIRECTORY)
    {
        return -1;
    }

    /* Create mount entry */
    vfs_mount_t *mount = kmalloc(sizeof(vfs_mount_t));
    if (!mount)
    {
        return -1;
    }

    strncpy(mount->path, path, 255);
    mount->path[255] = '\0';
    mount->mount_point = mount_point;
    mount->root = root;
    mount->fs_type = fs_type;
    mount->next = mount_list;

    /* Link to mount point */
    mount_point->mounted = root;
    root->parent = mount_point;

    /* Add to mount list */
    mount_list = mount;

    return 0;
}

int vfs_unmount(const char *path)
{
    vfs_mount_t *prev = NULL;
    vfs_mount_t *current = mount_list;

    while (current)
    {
        if (strcmp(current->path, path) == 0)
        {
            /* Found it - remove from list */
            if (prev)
            {
                prev->next = current->next;
            }
            else
            {
                mount_list = current->next;
            }

            /* Unlink mount point */
            current->mount_point->mounted = NULL;

            /* Free mount entry */
            kfree(current);

            return 0;
        }

        prev = current;
        current = current->next;
    }

    return -1; /* Mount not found */
}

/* ====================================================================
 * UTILITY FUNCTIONS
 * ==================================================================== */

int vfs_stat(const char *path, vfs_node_t *stat_buf)
{
    vfs_node_t *node = vfs_resolve_path(path);
    if (!node)
    {
        return -1;
    }

    /* Copy node info to stat buffer */
    memcpy(stat_buf, node, sizeof(vfs_node_t));
    return 0;
}

bool vfs_exists(const char *path)
{
    return vfs_resolve_path(path) != NULL;
}

const char *vfs_getcwd(void)
{
    /* TODO: Return actual path, not just name */
    return vfs_cwd ? vfs_cwd->name : "/";
}

int vfs_chdir(const char *path)
{
    vfs_node_t *node = vfs_resolve_path(path);
    if (!node || node->type != VFS_DIRECTORY)
    {
        return -1;
    }

    vfs_cwd = node;
    return 0;
}

/* ====================================================================
 * INITIALIZATION
 * ==================================================================== */

void vfs_init(void)
{
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("[VFS] Initializing Virtual File System...\n");

    /* Clear file descriptor table */
    memset(fd_table, 0, sizeof(fd_table));

    /* Root and cwd will be set when a filesystem is mounted */
    vfs_root = NULL;
    vfs_cwd = NULL;
    mount_list = NULL;

    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("[VFS] Virtual File System initialized\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}