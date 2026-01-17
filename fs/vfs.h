/* fs/vfs.h - Virtual File System Interface
 *
 * The VFS provides a unified interface for all filesystem types.
 * It abstracts the underlying filesystem implementation, allowing
 * multiple filesystem types (ramfs, tarfs, ext2, etc.) to coexist.
 *
 * DESIGN PHILOSOPHY:
 * "Everything is a file" - Unix philosophy
 * Files, directories, devices all use the same interface.
 *
 * KEY CONCEPTS:
 * - vfs_node: Represents any file system object (file, dir, device)
 * - vfs_operations: Function pointers for filesystem-specific operations
 * - File descriptors: Integer handles to open files
 * - Path resolution: Convert "/path/to/file" to vfs_node
 */

#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ====================================================================
 * VFS NODE TYPES
 * ==================================================================== */

typedef enum
{
    VFS_FILE = 0x01,        /* Regular file */
    VFS_DIRECTORY = 0x02,   /* Directory */
    VFS_CHARDEVICE = 0x03,  /* Character device (keyboard, terminal) */
    VFS_BLOCKDEVICE = 0x04, /* Block device (disk) */
    VFS_PIPE = 0x05,        /* Pipe (for IPC) */
    VFS_SYMLINK = 0x06,     /* Symbolic link */
    VFS_MOUNTPOINT = 0x08   /* Mount point */
} vfs_node_type_t;

/* ====================================================================
 * FILE OPEN FLAGS
 * ==================================================================== */

#define O_RDONLY 0x0000    /* Read-only */
#define O_WRONLY 0x0001    /* Write-only */
#define O_RDWR 0x0002      /* Read-write */
#define O_CREAT 0x0100     /* Create if doesn't exist */
#define O_TRUNC 0x0200     /* Truncate to 0 length */
#define O_APPEND 0x0400    /* Append mode */
#define O_DIRECTORY 0x0800 /* Must be directory */

/* ====================================================================
 * SEEK MODES
 * ==================================================================== */

#define SEEK_SET 0 /* Seek from beginning */
#define SEEK_CUR 1 /* Seek from current position */
#define SEEK_END 2 /* Seek from end */

/* ====================================================================
 * FILE PERMISSIONS (Unix-style)
 * ==================================================================== */

#define S_IRWXU 0x01C0 /* User read/write/execute */
#define S_IRUSR 0x0100 /* User read */
#define S_IWUSR 0x0080 /* User write */
#define S_IXUSR 0x0040 /* User execute */
#define S_IRWXG 0x0038 /* Group read/write/execute */
#define S_IRGRP 0x0020 /* Group read */
#define S_IWGRP 0x0010 /* Group write */
#define S_IXGRP 0x0008 /* Group execute */
#define S_IRWXO 0x0007 /* Other read/write/execute */
#define S_IROTH 0x0004 /* Other read */
#define S_IWOTH 0x0002 /* Other write */
#define S_IXOTH 0x0001 /* Other execute */

/* ====================================================================
 * FORWARD DECLARATIONS
 * ==================================================================== */

struct vfs_node;
struct dirent;

/* ====================================================================
 * VFS OPERATIONS
 *
 * Function pointers for filesystem-specific implementations.
 * Each filesystem (ramfs, tarfs, etc.) provides its own callbacks.
 * ==================================================================== */

typedef struct vfs_operations
{
    /* Open a file/directory
     * Returns 0 on success, -1 on error */
    int (*open)(struct vfs_node *node, uint32_t flags);

    /* Close a file/directory
     * Returns 0 on success, -1 on error */
    int (*close)(struct vfs_node *node);

    /* Read from file
     * Returns number of bytes read, or -1 on error */
    int (*read)(struct vfs_node *node, uint32_t offset, uint32_t size, uint8_t *buffer);

    /* Write to file
     * Returns number of bytes written, or -1 on error */
    int (*write)(struct vfs_node *node, uint32_t offset, uint32_t size, const uint8_t *buffer);

    /* Read directory entry by index
     * Returns pointer to dirent, or NULL if index >= num entries */
    struct dirent *(*readdir)(struct vfs_node *node, uint32_t index);

    /* Find child node by name in directory
     * Returns child node, or NULL if not found */
    struct vfs_node *(*finddir)(struct vfs_node *node, const char *name);

    /* Create a new file in directory
     * Returns new node, or NULL on error */
    struct vfs_node *(*create)(struct vfs_node *parent, const char *name, uint32_t mode);

    /* Delete a file
     * Returns 0 on success, -1 on error */
    int (*unlink)(struct vfs_node *parent, const char *name);

    /* Create a directory
     * Returns new directory node, or NULL on error */
    struct vfs_node *(*mkdir)(struct vfs_node *parent, const char *name, uint32_t mode);

    /* Remove a directory
     * Returns 0 on success, -1 on error */
    int (*rmdir)(struct vfs_node *parent, const char *name);

} vfs_operations_t;

/* ====================================================================
 * VFS NODE
 *
 * Universal representation of a filesystem object.
 * Can be a file, directory, device, etc.
 * ==================================================================== */

typedef struct vfs_node
{
    char name[256];       /* Node name (not full path) */
    uint32_t inode;       /* Inode number (filesystem-specific) */
    uint32_t size;        /* File size in bytes */
    uint32_t mode;        /* Permissions (Unix-style) */
    uint32_t uid;         /* User ID (owner) */
    uint32_t gid;         /* Group ID */
    uint32_t flags;       /* Open flags */
    vfs_node_type_t type; /* Node type (file, dir, device, etc.) */
    uint32_t open_count;  /* Reference count */

    /* Filesystem-specific data
     * ramfs: Pointer to data buffer or children list
     * tarfs: Offset in tar archive
     * ext2: Inode data structure */
    void *impl_data;

    /* Operations table - points to filesystem-specific functions */
    vfs_operations_t *ops;

    /* For directory traversal */
    struct vfs_node *parent; /* Parent directory */
    struct vfs_node *next;   /* Next sibling (in parent's children list) */

    /* Mount information */
    struct vfs_node *mounted; /* If this is a mount point, points to mounted root */

} vfs_node_t;

/* ====================================================================
 * DIRECTORY ENTRY
 *
 * Returned by readdir() - information about files in a directory
 * ==================================================================== */

typedef struct dirent
{
    char name[256]; /* File name */
    uint32_t inode; /* Inode number */
    uint32_t type;  /* File type (VFS_FILE, VFS_DIRECTORY, etc.) */
} dirent_t;

/* ====================================================================
 * FILE DESCRIPTOR
 *
 * User-facing handle to an open file.
 * Maps FD number → vfs_node + current position
 * ==================================================================== */

typedef struct file_descriptor
{
    vfs_node_t *node;  /* Pointer to vfs_node */
    uint32_t position; /* Current read/write position */
    uint32_t flags;    /* Open flags (O_RDONLY, etc.) */
    bool in_use;       /* Is this FD slot allocated? */
} file_descriptor_t;

/* ====================================================================
 * VFS MOUNT POINT
 * ==================================================================== */

typedef struct vfs_mount
{
    char path[256];          /* Mount path (e.g., "/mnt/disk") */
    vfs_node_t *mount_point; /* Directory where mounted */
    vfs_node_t *root;        /* Root of mounted filesystem */
    const char *fs_type;     /* Filesystem type ("ramfs", "tarfs", etc.) */
    struct vfs_mount *next;  /* Next mount in list */
} vfs_mount_t;

/* ====================================================================
 * VFS INITIALIZATION
 * ==================================================================== */

/* Initialize the VFS subsystem
 * Sets up root filesystem and file descriptor table */
void vfs_init(void);

/* ====================================================================
 * FILE OPERATIONS (User-facing API)
 * ==================================================================== */

/* Open a file/directory
 *
 * path: Full path like "/home/user/file.txt"
 * flags: O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, etc.
 *
 * Returns: File descriptor (>= 0), or -1 on error
 */
int vfs_open(const char *path, uint32_t flags);

/* Close a file descriptor
 *
 * fd: File descriptor from vfs_open()
 *
 * Returns: 0 on success, -1 on error
 */
int vfs_close(int fd);

/* Read from file
 *
 * fd: File descriptor
 * buffer: Where to store read data
 * size: How many bytes to read
 *
 * Returns: Number of bytes actually read, or -1 on error
 */
int vfs_read(int fd, void *buffer, uint32_t size);

/* Write to file
 *
 * fd: File descriptor
 * buffer: Data to write
 * size: How many bytes to write
 *
 * Returns: Number of bytes actually written, or -1 on error
 */
int vfs_write(int fd, const void *buffer, uint32_t size);

/* Seek to position in file
 *
 * fd: File descriptor
 * offset: Offset to seek to
 * whence: SEEK_SET, SEEK_CUR, or SEEK_END
 *
 * Returns: New position, or -1 on error
 */
int vfs_seek(int fd, int32_t offset, int whence);

/* Read directory entry
 *
 * fd: File descriptor (must be a directory)
 * index: Entry index (0, 1, 2, ...)
 *
 * Returns: Pointer to dirent, or NULL if no more entries
 */
dirent_t *vfs_readdir(int fd, uint32_t index);

/* ====================================================================
 * DIRECTORY OPERATIONS
 * ==================================================================== */

/* Create a directory
 *
 * path: Full path like "/home/user/newdir"
 * mode: Permissions (Unix-style)
 *
 * Returns: 0 on success, -1 on error
 */
int vfs_mkdir(const char *path, uint32_t mode);

/* Remove a directory (must be empty)
 *
 * path: Full path
 *
 * Returns: 0 on success, -1 on error
 */
int vfs_rmdir(const char *path);

/* Delete a file
 *
 * path: Full path
 *
 * Returns: 0 on success, -1 on error
 */
int vfs_unlink(const char *path);

/* ====================================================================
 * PATH RESOLUTION
 * ==================================================================== */

/* Resolve a path to a vfs_node
 *
 * path: Full path like "/home/user/file.txt"
 *
 * Returns: vfs_node*, or NULL if not found
 *
 * This is the core of VFS - it walks the directory tree
 * and handles mount points.
 */
vfs_node_t *vfs_resolve_path(const char *path);

/* ====================================================================
 * MOUNT OPERATIONS
 * ==================================================================== */

/* Mount a filesystem
 *
 * device: Device node (e.g., "/dev/hda1") or NULL for pseudo-fs
 * path: Where to mount (e.g., "/mnt")
 * fs_type: Filesystem type ("ramfs", "tarfs", "ext2", etc.)
 * root: Root node of the filesystem to mount
 *
 * Returns: 0 on success, -1 on error
 */
int vfs_mount(const char *device, const char *path, const char *fs_type, vfs_node_t *root);

/* Unmount a filesystem
 *
 * path: Mount point
 *
 * Returns: 0 on success, -1 on error
 */
int vfs_unmount(const char *path);

/* ====================================================================
 * UTILITY FUNCTIONS
 * ==================================================================== */

/* Get file statistics (like Unix stat())
 * Returns size, type, permissions, etc. */
int vfs_stat(const char *path, vfs_node_t *stat_buf);

/* Check if file exists */
bool vfs_exists(const char *path);

/* Get current working directory */
const char *vfs_getcwd(void);

/* Change current working directory */
int vfs_chdir(const char *path);

/* ====================================================================
 * GLOBALS
 * ==================================================================== */

/* Root filesystem node (/) */
extern vfs_node_t *vfs_root;

/* Current working directory */
extern vfs_node_t *vfs_cwd;

/* Maximum number of open files per process */
#define MAX_OPEN_FILES 256

#endif /* VFS_H */