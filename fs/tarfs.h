/* fs/tarfs.h - Tar Filesystem Driver
 *
 * TarFS reads tar archives from disk and presents them as a filesystem.
 * 
 * TAR FORMAT (USTAR):
 * - Each file has a 512-byte header
 * - File data follows header (rounded to 512 bytes)
 * - Simple, sequential format
 * - Easy to parse!
 *
 * WHY TARFS?
 * - Simple format (easier than ext2/FAT)
 * - Can create tar files on host OS
 * - Good for read-only root filesystem
 * - Can add write support later
 *
 * LIMITATIONS:
 * - Read-only (initially)
 * - No compression support
 * - Entire archive loaded to RAM
 */

#ifndef TARFS_H
#define TARFS_H

#include <stdint.h>
#include <stdbool.h>
#include "vfs.h"

/* ====================================================================
 * TAR HEADER FORMAT (USTAR)
 * 
 * This is the exact structure of a tar header (512 bytes).
 * All fields are ASCII text!
 * ==================================================================== */

typedef struct tar_header {
    char filename[100];      /* File name */
    char mode[8];            /* File mode (octal) */
    char uid[8];             /* Owner user ID (octal) */
    char gid[8];             /* Owner group ID (octal) */
    char size[12];           /* File size in bytes (octal) */
    char mtime[12];          /* Last modification time (octal) */
    char checksum[8];        /* Header checksum (octal) */
    char typeflag;           /* File type */
    char linkname[100];      /* Target of symbolic link */
    char magic[6];           /* "ustar\0" */
    char version[2];         /* "00" */
    char uname[32];          /* Owner user name */
    char gname[32];          /* Owner group name */
    char devmajor[8];        /* Device major number */
    char devminor[8];        /* Device minor number */
    char prefix[155];        /* Filename prefix */
    char padding[12];        /* Padding to 512 bytes */
} __attribute__((packed)) tar_header_t;

/* Tar file type flags */
#define TAR_TYPE_FILE          '0'     /* Regular file */
#define TAR_TYPE_HARDLINK      '1'     /* Hard link */
#define TAR_TYPE_SYMLINK       '2'     /* Symbolic link */
#define TAR_TYPE_CHARDEV       '3'     /* Character device */
#define TAR_TYPE_BLOCKDEV      '4'     /* Block device */
#define TAR_TYPE_DIRECTORY     '5'     /* Directory */
#define TAR_TYPE_FIFO          '6'     /* FIFO */

/* ====================================================================
 * TARFS FUNCTIONS
 * ==================================================================== */

/* Initialize TarFS subsystem */
void tarfs_init(void);

/* Load a tar archive from disk
 * 
 * drive: ATA drive number (0 = Primary Master)
 * start_lba: Starting sector of tar archive
 * 
 * Returns: Root vfs_node of filesystem, or NULL on error
 */
vfs_node_t *tarfs_load(uint8_t drive, uint32_t start_lba);

/* Parse a tar archive from memory buffer
 *
 * buffer: Pointer to tar data in memory
 * size: Size of tar data in bytes
 *
 * Returns: Root vfs_node of filesystem, or NULL on error
 */
vfs_node_t *tarfs_parse(const uint8_t *buffer, size_t size);

/* ====================================================================
 * UTILITY FUNCTIONS
 * ==================================================================== */

/* Convert octal string to integer
 * Tar stores numbers as octal ASCII strings! */
uint32_t tar_octal_to_uint(const char *str, size_t len);

/* Verify tar header checksum
 * Returns true if valid */
bool tar_verify_checksum(const tar_header_t *header);

#endif /* TARFS_H */