/* fs/fat.h - FAT16 Filesystem Driver
 *
 * Implements FAT16 (File Allocation Table 16-bit).
 * 
 * WHY FAT16?
 * - Simple and well-documented
 * - Compatible with all operating systems
 * - You can mount disk.img on your host machine!
 * - Perfect size for small disks (up to 2GB)
 * - No complex features to implement
 *
 * FAT16 DISK LAYOUT:
 * ┌────────────┬──────┬──────┬──────────┬─────────────┐
 * │ Boot       │ FAT1 │ FAT2 │ Root Dir │ Data Area   │
 * │ Sector (1) │      │(copy)│          │ (clusters)  │
 * └────────────┴──────┴──────┴──────────┴─────────────┘
 *
 * CLUSTERS:
 * - Data area divided into clusters (groups of sectors)
 * - Each cluster is 4 sectors = 2KB
 * - FAT table maps cluster chains
 */

#ifndef FAT_H
#define FAT_H

#include <stdint.h>
#include <stdbool.h>
#include "vfs.h"

/* ====================================================================
 * FAT BOOT SECTOR (512 bytes)
 * First sector of the disk - contains filesystem metadata
 * ==================================================================== */

typedef struct __attribute__((packed)) fat_boot_sector {
    uint8_t  jump[3];              /* Jump instruction to boot code */
    char     oem_name[8];          /* OEM name (e.g., "OSCOMPLEX") */
    uint16_t bytes_per_sector;     /* Usually 512 */
    uint8_t  sectors_per_cluster;  /* Usually 4 (2KB clusters) */
    uint16_t reserved_sectors;     /* Usually 1 (just boot sector) */
    uint8_t  num_fats;             /* Usually 2 (main + backup) */
    uint16_t root_entries;         /* Number of root directory entries */
    uint16_t total_sectors_16;     /* Total sectors (if < 65536) */
    uint8_t  media_descriptor;     /* Media type (0xF8 = hard disk) */
    uint16_t sectors_per_fat;      /* Size of one FAT */
    uint16_t sectors_per_track;    /* For CHS addressing */
    uint16_t num_heads;            /* For CHS addressing */
    uint32_t hidden_sectors;       /* Sectors before partition */
    uint32_t total_sectors_32;     /* Total sectors (if >= 65536) */
    
    /* Extended boot record (FAT16 specific) */
    uint8_t  drive_number;         /* 0x80 = hard disk */
    uint8_t  reserved;             /* Reserved */
    uint8_t  boot_signature;       /* 0x29 = extended boot signature */
    uint32_t volume_id;            /* Volume serial number */
    char     volume_label[11];     /* Volume label */
    char     fs_type[8];           /* "FAT16   " */
    
    uint8_t  boot_code[448];       /* Boot code (we don't use) */
    uint16_t boot_signature_end;   /* 0xAA55 - boot signature */
} fat_boot_sector_t;

/* ====================================================================
 * FAT DIRECTORY ENTRY (32 bytes)
 * Each file/directory has one of these
 * ==================================================================== */

typedef struct __attribute__((packed)) fat_dir_entry {
    char     name[11];             /* 8.3 filename (padded with spaces) */
    uint8_t  attributes;           /* File attributes */
    uint8_t  reserved;             /* Reserved for Windows NT */
    uint8_t  creation_time_tenth;  /* Creation time, tenths of second */
    uint16_t creation_time;        /* Creation time */
    uint16_t creation_date;        /* Creation date */
    uint16_t access_date;          /* Last access date */
    uint16_t first_cluster_high;   /* High word of first cluster (FAT32) */
    uint16_t modified_time;        /* Last modified time */
    uint16_t modified_date;        /* Last modified date */
    uint16_t first_cluster;        /* Low word of first cluster */
    uint32_t file_size;            /* File size in bytes */
} fat_dir_entry_t;

/* ====================================================================
 * FAT ATTRIBUTES
 * ==================================================================== */

#define FAT_ATTR_READ_ONLY  0x01   /* Read-only file */
#define FAT_ATTR_HIDDEN     0x02   /* Hidden file */
#define FAT_ATTR_SYSTEM     0x04   /* System file */
#define FAT_ATTR_VOLUME_ID  0x08   /* Volume label */
#define FAT_ATTR_DIRECTORY  0x10   /* Directory */
#define FAT_ATTR_ARCHIVE    0x20   /* Archive (modified since backup) */

/* Long filename entry (we won't implement LFN for now) */
#define FAT_ATTR_LONG_NAME  (FAT_ATTR_READ_ONLY | FAT_ATTR_HIDDEN | \
                             FAT_ATTR_SYSTEM | FAT_ATTR_VOLUME_ID)

/* ====================================================================
 * FAT CLUSTER VALUES
 * ==================================================================== */

#define FAT_CLUSTER_FREE     0x0000  /* Cluster is available */
#define FAT_CLUSTER_RESERVED 0x0001  /* Reserved cluster */
#define FAT_CLUSTER_BAD      0xFFF7  /* Bad cluster */
#define FAT_CLUSTER_EOC      0xFFF8  /* End of chain (0xFFF8-0xFFFF) */

/* ====================================================================
 * FAT FILESYSTEM STATE
 * ==================================================================== */

typedef struct fat_fs {
    uint8_t drive;                 /* ATA drive number */
    uint32_t partition_start;      /* LBA of partition start */
    
    /* Boot sector info */
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t sectors_per_fat;
    uint32_t total_sectors;
    
    /* Calculated values */
    uint32_t fat_start;            /* LBA of first FAT */
    uint32_t root_dir_start;       /* LBA of root directory */
    uint32_t data_start;           /* LBA of data area */
    uint32_t total_clusters;       /* Number of data clusters */
    
    /* Cached FAT table (loaded into memory) */
    uint16_t *fat_table;           /* Pointer to FAT in memory */
    bool fat_dirty;                /* FAT needs to be written back */
    
} fat_fs_t;

/* ====================================================================
 * FAT NODE DATA
 * Stored in vfs_node->impl_data
 * ==================================================================== */

typedef struct fat_node_data {
    uint16_t first_cluster;        /* First cluster of file/directory */
    uint32_t dir_entry_sector;     /* Sector containing directory entry */
    uint32_t dir_entry_offset;     /* Offset within sector */
} fat_node_data_t;

/* ====================================================================
 * FUNCTION PROTOTYPES
 * ==================================================================== */

/* Initialize FAT driver */
void fat_init(void);

/* Mount a FAT filesystem from disk
 * 
 * drive: ATA drive number (0 = Primary Master)
 * partition_start: LBA where partition starts (usually 0)
 * 
 * Returns: Root vfs_node of filesystem, or NULL on error
 */
vfs_node_t *fat_mount(uint8_t drive, uint32_t partition_start);

/* Unmount FAT filesystem (flushes changes) */
void fat_unmount(vfs_node_t *root);

/* Flush FAT table back to disk */
int fat_sync(void);

/* ====================================================================
 * UTILITY FUNCTIONS
 * ==================================================================== */

/* Convert 8.3 filename to normal string */
void fat_filename_to_str(const char *fat_name, char *output);

/* Convert normal string to 8.3 filename */
void fat_str_to_filename(const char *input, char *fat_name);

/* Check if directory entry is valid */
bool fat_is_valid_entry(const fat_dir_entry_t *entry);

#endif /* FAT_H */