// src/fs/fat32.h
#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stddef.h>

#pragma pack(push, 1)

// Структура Boot Sector (BPB)
typedef struct {
    uint8_t  jump[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t dir_entries;
    uint16_t total_sectors_16;
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    // Специфично для FAT32
    uint32_t sectors_per_fat_32;
    uint16_t flags;
    uint16_t fat_version;
    uint32_t root_cluster;
    uint16_t fs_info_cluster;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} fat32_bpb_t;

// Структура записи о файле (Directory Entry)
typedef struct {
    char     name[11];         // Имя 8 байт + Расширение 3 байта
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t access_date;
    uint16_t cluster_high;     // Старшие 16 бит номера кластера
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t cluster_low;      // Младшие 16 бит номера кластера
    uint32_t size;             // Размер файла в байтах
} fat32_dir_entry_t;

#pragma pack(pop)

void fat32_init(void);
uint8_t* fat32_read_file(const char* filename, uint32_t* out_size);

#endif