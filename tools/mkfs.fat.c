/* tools/mkfs.fat.c - Create FAT16 Filesystem
 *
 * Formats a disk image as FAT16.
 * Run on your host machine (Linux/WSL).
 *
 * Usage: gcc -o mkfs.fat mkfs.fat.c && ./mkfs.fat disk.img
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#pragma pack(push, 1)

typedef struct {
    uint8_t  jump[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t  drive_number;
    uint8_t  reserved;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
    uint8_t  boot_code[448];
    uint16_t boot_sig_end;
} fat_boot_sector_t;

typedef struct {
    char     name[11];
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  creation_time_tenth;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t access_date;
    uint16_t first_cluster_high;
    uint16_t modified_time;
    uint16_t modified_date;
    uint16_t first_cluster;
    uint32_t file_size;
} fat_dir_entry_t;

#pragma pack(pop)

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <disk_image>\n", argv[0]);
        printf("Creates a FAT16 filesystem on the disk image.\n");
        return 1;
    }
    
    const char *filename = argv[1];
    
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║              OSComplex FAT16 Formatter                   ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n\n");
    
    FILE *disk = fopen(filename, "r+b");
    if (!disk) {
        printf("ERROR: Cannot open %s\n", filename);
        return 1;
    }
    
    /* Get disk size */
    fseek(disk, 0, SEEK_END);
    long disk_size = ftell(disk);
    fseek(disk, 0, SEEK_SET);
    
    printf("[INFO] Disk image: %s\n", filename);
    printf("[INFO] Size: %ld bytes (%ld MB)\n", disk_size, disk_size / (1024*1024));
    
    /* FAT16 parameters */
    uint16_t bytes_per_sector = 512;
    uint8_t  sectors_per_cluster = 4;      /* 2KB clusters */
    uint16_t reserved_sectors = 1;
    uint8_t  num_fats = 2;
    uint16_t root_entries = 512;
    uint32_t total_sectors = disk_size / bytes_per_sector;
    
    /* Calculate FAT size */
    uint32_t root_dir_sectors = (root_entries * 32 + bytes_per_sector - 1) / bytes_per_sector;
    uint32_t tmp1 = total_sectors - (reserved_sectors + root_dir_sectors);
    uint32_t tmp2 = (256 * sectors_per_cluster) + num_fats;
    uint16_t sectors_per_fat = (tmp1 + tmp2 - 1) / tmp2;
    
    uint32_t fat_size = reserved_sectors + (num_fats * sectors_per_fat);
    uint32_t data_start = fat_size + root_dir_sectors;
    uint32_t data_sectors = total_sectors - data_start;
    uint32_t total_clusters = data_sectors / sectors_per_cluster;
    
    printf("\n[FORMAT] Creating FAT16 filesystem:\n");
    printf("  Bytes/sector:     %d\n", bytes_per_sector);
    printf("  Sectors/cluster:  %d\n", sectors_per_cluster);
    printf("  Reserved sectors: %d\n", reserved_sectors);
    printf("  FATs:             %d\n", num_fats);
    printf("  Root entries:     %d\n", root_entries);
    printf("  Total sectors:    %u\n", total_sectors);
    printf("  Sectors/FAT:      %d\n", sectors_per_fat);
    printf("  Total clusters:   %u\n", total_clusters);
    
    if (total_clusters < 4085 || total_clusters >= 65525) {
        printf("\nERROR: Cluster count %u is not valid for FAT16\n", total_clusters);
        printf("FAT16 requires 4085-65524 clusters\n");
        fclose(disk);
        return 1;
    }
    
    /* Create boot sector */
    fat_boot_sector_t boot;
    memset(&boot, 0, sizeof(boot));
    
    boot.jump[0] = 0xEB;
    boot.jump[1] = 0x3C;
    boot.jump[2] = 0x90;
    
    memcpy(boot.oem_name, "OSCOMPLEX", 8);
    boot.bytes_per_sector = bytes_per_sector;
    boot.sectors_per_cluster = sectors_per_cluster;
    boot.reserved_sectors = reserved_sectors;
    boot.num_fats = num_fats;
    boot.root_entries = root_entries;
    boot.total_sectors_16 = (total_sectors < 65536) ? total_sectors : 0;
    boot.media_descriptor = 0xF8;  /* Hard disk */
    boot.sectors_per_fat = sectors_per_fat;
    boot.sectors_per_track = 63;
    boot.num_heads = 255;
    boot.hidden_sectors = 0;
    boot.total_sectors_32 = (total_sectors >= 65536) ? total_sectors : 0;
    
    boot.drive_number = 0x80;
    boot.reserved = 0;
    boot.boot_signature = 0x29;
    boot.volume_id = (uint32_t)time(NULL);
    memcpy(boot.volume_label, "OSCOMPLEX  ", 11);
    memcpy(boot.fs_type, "FAT16   ", 8);
    boot.boot_sig_end = 0xAA55;
    
    printf("\n[WRITE] Writing boot sector...\n");
    fwrite(&boot, sizeof(boot), 1, disk);
    
    /* Create FAT tables */
    printf("[WRITE] Writing FAT tables...\n");
    
    uint16_t *fat = calloc(sectors_per_fat * bytes_per_sector / 2, sizeof(uint16_t));
    fat[0] = 0xFFF8;  /* Media descriptor */
    fat[1] = 0xFFFF;  /* End of chain */
    
    for (int i = 0; i < num_fats; i++) {
        fseek(disk, (reserved_sectors + i * sectors_per_fat) * bytes_per_sector, SEEK_SET);
        fwrite(fat, sectors_per_fat * bytes_per_sector, 1, disk);
    }
    
    free(fat);
    
    /* Create root directory */
    printf("[WRITE] Writing root directory...\n");
    
    fseek(disk, (reserved_sectors + num_fats * sectors_per_fat) * bytes_per_sector, SEEK_SET);
    fat_dir_entry_t *root = calloc(root_entries, sizeof(fat_dir_entry_t));
    fwrite(root, root_entries * sizeof(fat_dir_entry_t), 1, disk);
    free(root);
    
    /* Zero out data area (optional but recommended) */
    printf("[WRITE] Clearing data area...\n");
    
    uint8_t *zero_sector = calloc(bytes_per_sector, 1);
    fseek(disk, data_start * bytes_per_sector, SEEK_SET);
    
    for (uint32_t i = 0; i < data_sectors && i < 1000; i++) {
        fwrite(zero_sector, bytes_per_sector, 1, disk);
    }
    
    free(zero_sector);
    
    fclose(disk);
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║              FAT16 Filesystem Created!                   ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\nYou can now:\n");
    printf("  1. Mount in Linux: sudo mount -o loop %s /mnt\n", filename);
    printf("  2. Add files in Linux\n");
    printf("  3. Boot OSComplex and access files!\n\n");
    
    return 0;
}