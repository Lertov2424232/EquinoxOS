#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>

#pragma pack(push, 1)
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
    // FAT32 Extended fields
    uint32_t sectors_per_fat_32;
    uint16_t flags;
    uint16_t fat_version;
    uint32_t root_cluster;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_num;
    uint8_t  reserved1;
    uint8_t  boot_sig;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} fat32_bpb_t;

typedef struct {
    char     name[11];
    uint8_t  attr;
    uint8_t  nt_res;
    uint8_t  crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t lst_acc_date;
    uint16_t cluster_high;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t cluster_low;
    uint32_t file_size;
} fat32_entry_t;

typedef struct {
    char name[13];
    uint32_t size;
    uint32_t cluster;
} fat32_file_info_t;
#pragma pack(pop)

void fat32_init(void);
uint8_t* fat32_read_file(const char* name, uint32_t* out_size);
void fat32_list_files();
int fat32_get_files(fat32_file_info_t* out_list, int max_files);
uint32_t fat32_find_free_cluster();
void fat32_set_cluster_entry(uint32_t cluster, uint32_t value);
void fat32_save_file(const char* name, const char* data, uint32_t size);
void fat32_to_83(const char* src, char* dst);
uint32_t fat32_allocate_cluster_chain(uint32_t count);

#endif