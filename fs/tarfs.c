/* fs/tarfs.c - Tar Filesystem Implementation
 *
 * Reads tar archives from disk and makes them accessible via VFS.
 * 
 * HOW IT WORKS:
 * 1. Load tar archive from disk (via ATA driver)
 * 2. Parse 512-byte tar headers
 * 3. Build directory tree in RAM
 * 4. Serve files through VFS callbacks
 *
 * MEMORY USAGE:
 * - Entire tar archive is loaded into RAM
 * - Directory structure is built from headers
 * - File data stays in the loaded buffer
 */

#include "tarfs.h"
#include "vfs.h"
#include "../kernel/kernel.h"
#include "../drivers/ata.h"

/* ====================================================================
 * TARFS NODE DATA
 * 
 * Each vfs_node's impl_data points to this structure
 * ==================================================================== */

typedef struct tarfs_node_data {
    const uint8_t *data;     /* Pointer to file data in tar buffer */
    size_t offset;           /* Offset in tar archive */
    vfs_node_t *children;    /* For directories: linked list of children */
} tarfs_node_data_t;

/* ====================================================================
 * UTILITY FUNCTIONS
 * ==================================================================== */

/* Convert octal ASCII string to uint32_t
 * Tar stores all numbers as octal! */
uint32_t tar_octal_to_uint(const char *str, size_t len) {
    uint32_t value = 0;
    
    for (size_t i = 0; i < len && str[i] >= '0' && str[i] <= '7'; i++) {
        value = (value * 8) + (str[i] - '0');
    }
    
    return value;
}

/* Verify tar header checksum */
bool tar_verify_checksum(const tar_header_t *header) {
    /* Calculate checksum */
    uint32_t unsigned_sum = 0;
    const uint8_t *bytes = (const uint8_t *)header;
    
    /* Sum all bytes, treating checksum field as spaces */
    for (int i = 0; i < 512; i++) {
        if (i >= 148 && i < 156) {
            unsigned_sum += ' ';  /* Checksum field */
        } else {
            unsigned_sum += bytes[i];
        }
    }
    
    /* Get stored checksum */
    uint32_t stored_checksum = tar_octal_to_uint(header->checksum, 8);
    
    return unsigned_sum == stored_checksum;
}

/* ====================================================================
 * VFS OPERATIONS
 * ==================================================================== */

static int tarfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (!node || node->type != VFS_FILE) {
        return -1;
    }
    
    tarfs_node_data_t *data = (tarfs_node_data_t *)node->impl_data;
    if (!data || !data->data) {
        return 0;
    }
    
    /* Don't read past end of file */
    if (offset >= node->size) {
        return 0;
    }
    
    uint32_t bytes_to_read = size;
    if (offset + bytes_to_read > node->size) {
        bytes_to_read = node->size - offset;
    }
    
    /* Copy data */
    memcpy(buffer, data->data + offset, bytes_to_read);
    
    return bytes_to_read;
}

static dirent_t *tarfs_readdir(vfs_node_t *node, uint32_t index) {
    if (!node || node->type != VFS_DIRECTORY) {
        return NULL;
    }
    
    static dirent_t dent;
    
    tarfs_node_data_t *data = (tarfs_node_data_t *)node->impl_data;
    if (!data) return NULL;
    
    /* Walk to requested index */
    vfs_node_t *child = data->children;
    for (uint32_t i = 0; i < index && child; i++) {
        child = child->next;
    }
    
    if (!child) return NULL;
    
    /* Fill dirent */
    strncpy(dent.name, child->name, sizeof(dent.name) - 1);
    dent.name[sizeof(dent.name) - 1] = '\0';
    dent.inode = child->inode;
    dent.type = child->type;
    
    return &dent;
}

static vfs_node_t *tarfs_finddir(vfs_node_t *node, const char *name) {
    if (!node || node->type != VFS_DIRECTORY) {
        return NULL;
    }
    
    tarfs_node_data_t *data = (tarfs_node_data_t *)node->impl_data;
    if (!data) return NULL;
    
    /* Search children */
    for (vfs_node_t *child = data->children; child; child = child->next) {
        if (strcmp(child->name, name) == 0) {
            return child;
        }
    }
    
    return NULL;
}

/* TarFS operations table - read-only filesystem */
static vfs_operations_t tarfs_ops = {
    .open = NULL,
    .close = NULL,
    .read = tarfs_read,
    .write = NULL,              /* Read-only */
    .readdir = tarfs_readdir,
    .finddir = tarfs_finddir,
    .create = NULL,             /* Read-only */
    .unlink = NULL,             /* Read-only */
    .mkdir = NULL,              /* Read-only */
    .rmdir = NULL,              /* Read-only */
};

/* ====================================================================
 * DIRECTORY TREE BUILDING
 * ==================================================================== */

/* Find or create directory node by path */
static vfs_node_t *tarfs_get_or_create_dir(vfs_node_t *root, const char *path) {
    if (!path || !*path || strcmp(path, "/") == 0) {
        return root;
    }
    
    /* Split path into components */
    char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    
    vfs_node_t *current = root;
    char *saveptr;
    char *component = strtok_r(path_copy, "/", &saveptr);
    
    while (component) {
        /* Look for existing child */
        tarfs_node_data_t *current_data = (tarfs_node_data_t *)current->impl_data;
        vfs_node_t *child = NULL;
        
        for (vfs_node_t *c = current_data->children; c; c = c->next) {
            if (strcmp(c->name, component) == 0) {
                child = c;
                break;
            }
        }
        
        /* Create if doesn't exist */
        if (!child) {
            child = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
            memset(child, 0, sizeof(vfs_node_t));
            
            strncpy(child->name, component, sizeof(child->name) - 1);
            child->type = VFS_DIRECTORY;
            child->ops = &tarfs_ops;
            child->parent = current;
            
            tarfs_node_data_t *child_data = (tarfs_node_data_t *)kmalloc(sizeof(tarfs_node_data_t));
            memset(child_data, 0, sizeof(tarfs_node_data_t));
            child->impl_data = child_data;
            
            /* Add to parent's children list */
            child->next = current_data->children;
            current_data->children = child;
        }
        
        current = child;
        component = strtok_r(NULL, "/", &saveptr);
    }
    
    return current;
}

/* ====================================================================
 * TAR PARSING
 * ==================================================================== */

vfs_node_t *tarfs_parse(const uint8_t *buffer, size_t size) {
    /* Create root directory */
    vfs_node_t *root = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
    memset(root, 0, sizeof(vfs_node_t));
    
    root->name[0] = '\0';
    root->type = VFS_DIRECTORY;
    root->ops = &tarfs_ops;
    
    tarfs_node_data_t *root_data = (tarfs_node_data_t *)kmalloc(sizeof(tarfs_node_data_t));
    memset(root_data, 0, sizeof(tarfs_node_data_t));
    root->impl_data = root_data;
    
    /* Parse tar archive */
    size_t offset = 0;
    
    while (offset + 512 <= size) {
        const tar_header_t *header = (const tar_header_t *)(buffer + offset);
        
        /* Check for end of archive (all zeros) */
        if (header->filename[0] == '\0') {
            break;
        }
        
        /* Verify magic number */
        if (strncmp(header->magic, "ustar", 5) != 0) {
            /* Not a valid tar header */
            offset += 512;
            continue;
        }
        
        /* Get file size */
        uint32_t file_size = tar_octal_to_uint(header->size, 12);
        
        /* Calculate next header offset (file size rounded up to 512) */
        uint32_t file_blocks = (file_size + 511) / 512;
        
        /* Parse filename and create directory structure */
        char filepath[256];
        strncpy(filepath, header->filename, sizeof(filepath) - 1);
        filepath[sizeof(filepath) - 1] = '\0';
        
        /* Remove trailing slash from directories */
        size_t len = strlen(filepath);
        if (len > 0 && filepath[len - 1] == '/') {
            filepath[len - 1] = '\0';
        }
        
        /* Get parent directory */
        char *last_slash = strrchr(filepath, '/');
        vfs_node_t *parent;
        const char *filename;
        
        if (last_slash) {
            *last_slash = '\0';
            parent = tarfs_get_or_create_dir(root, filepath);
            filename = last_slash + 1;
        } else {
            parent = root;
            filename = filepath;
        }
        
        /* Create node */
        if (*filename) {  /* Skip if empty filename */
            vfs_node_t *node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
            memset(node, 0, sizeof(vfs_node_t));
            
            strncpy(node->name, filename, sizeof(node->name) - 1);
            node->parent = parent;
            node->ops = &tarfs_ops;
            
            if (header->typeflag == TAR_TYPE_DIRECTORY || header->typeflag == '5') {
                node->type = VFS_DIRECTORY;
                
                tarfs_node_data_t *node_data = (tarfs_node_data_t *)kmalloc(sizeof(tarfs_node_data_t));
                memset(node_data, 0, sizeof(tarfs_node_data_t));
                node->impl_data = node_data;
            } else {
                node->type = VFS_FILE;
                node->size = file_size;
                
                tarfs_node_data_t *node_data = (tarfs_node_data_t *)kmalloc(sizeof(tarfs_node_data_t));
                node_data->data = buffer + offset + 512;  /* Data starts after header */
                node_data->offset = offset + 512;
                node_data->children = NULL;
                node->impl_data = node_data;
            }
            
            /* Add to parent's children list */
            tarfs_node_data_t *parent_data = (tarfs_node_data_t *)parent->impl_data;
            node->next = parent_data->children;
            parent_data->children = node;
        }
        
        /* Move to next header */
        offset += 512 + (file_blocks * 512);
    }
    
    return root;
}

/* ====================================================================
 * DISK LOADING
 * ==================================================================== */

vfs_node_t *tarfs_load(uint8_t drive, uint32_t start_lba) {
    terminal_writestring("[TARFS] Loading tar archive from disk...\n");
    
    /* Read first sector to get archive size */
    uint8_t *first_sector = (uint8_t *)kmalloc(512);
    if (!first_sector || ata_read_sector(drive, start_lba, first_sector) < 0) {
        terminal_writestring("[TARFS] ERROR: Failed to read first sector\n");
        if (first_sector) kfree(first_sector);
        return NULL;
    }
    
    /* For simplicity, allocate a fixed size buffer (e.g., 1MB)
     * In production, you'd scan the tar to find the real size */
    size_t buffer_size = 1024 * 1024;  /* 1MB */
    uint8_t *buffer = (uint8_t *)kmalloc(buffer_size);
    if (!buffer) {
        terminal_writestring("[TARFS] ERROR: Out of memory\n");
        kfree(first_sector);
        return NULL;
    }
    
    /* Copy first sector */
    memcpy(buffer, first_sector, 512);
    kfree(first_sector);
    
    /* Read rest of archive */
    uint32_t sectors = buffer_size / 512;
    for (uint32_t i = 1; i < sectors; i++) {
        if (ata_read_sector(drive, start_lba + i, buffer + (i * 512)) < 0) {
            terminal_writestring("[TARFS] Warning: Failed to read sector ");
            terminal_write_dec(i);
            terminal_writestring("\n");
            break;
        }
    }
    
    terminal_writestring("[TARFS] Archive loaded, parsing...\n");
    
    /* Parse the tar archive */
    vfs_node_t *root = tarfs_parse(buffer, buffer_size);
    
    if (root) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("[TARFS] Tar filesystem mounted successfully\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    } else {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("[TARFS] ERROR: Failed to parse tar archive\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        kfree(buffer);
    }
    
    return root;
}

/* ====================================================================
 * INITIALIZATION
 * ==================================================================== */

void tarfs_init(void) {
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("[TARFS] Tar filesystem driver initialized\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}