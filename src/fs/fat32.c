// src/fs/fat32.c
#include "fat32.h"
#include "drivers/disk/ata.h"
#include "system/memory.h"
#include "libc/string.h"

extern void term_print(const char* str);

static uint32_t partition_lba = 0;
static uint32_t fat_lba = 0;
static uint32_t data_lba = 0;
static uint32_t root_cluster = 0;
static uint32_t sectors_per_cluster = 0;

// Чтение следующего кластера из таблицы FAT
static uint32_t get_next_cluster(uint32_t current_cluster) {
    uint32_t fat_offset = current_cluster * 4;
    uint32_t fat_sector = fat_lba + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;
    
    uint8_t sector_buf[512];
    read_sectors_ata_pio((uint64_t)sector_buf, fat_sector, 1);
    
    uint32_t next_cluster = *(uint32_t*)&sector_buf[ent_offset];
    return next_cluster & 0x0FFFFFFF; // Маскируем старшие 4 бита
}

// Конвертация строки "bg.bmp" -> "BG      BMP" (формат FAT)
static void format_fat_name(const char* name, char* fat_name) {
    memset(fat_name, ' ', 11);
    int i = 0, j = 0;
    while (name[i] != '.' && name[i] != '\0' && j < 8) {
        char c = name[i++];
        if (c >= 'a' && c <= 'z') c -= 32; // toupper
        fat_name[j++] = c;
    }
    if (name[i] == '.') {
        i++;
        j = 8;
        while (name[i] != '\0' && j < 11) {
            char c = name[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            fat_name[j++] = c;
        }
    }
}

void fat32_init() {
    uint8_t boot_sector[512];
    read_sectors_ata_pio((uint64_t)boot_sector, 0, 1);
    
    // Проверяем, есть ли таблица разделов MBR
    // Обычно код прыжка (JMP) в BPB это 0xEB или 0xE9
    if (boot_sector[0] != 0xEB && boot_sector[0] != 0xE9) {
        // Это MBR. Читаем первый раздел (смещение 0x1BE)
        uint32_t part1_lba = *(uint32_t*)(boot_sector + 0x1BE + 8);
        partition_lba = part1_lba;
        read_sectors_ata_pio((uint64_t)boot_sector, partition_lba, 1);
    }
    
    fat32_bpb_t* bpb = (fat32_bpb_t*)boot_sector;
    
    fat_lba = partition_lba + bpb->reserved_sectors;
    data_lba = fat_lba + (bpb->fat_count * bpb->sectors_per_fat_32);
    root_cluster = bpb->root_cluster;
    sectors_per_cluster = bpb->sectors_per_cluster;
    
    term_print("FAT32 Initialized. Ready to read files.\n");
}

// Загрузка файла целиком в ОЗУ (Возвращает указатель из кучи)
uint8_t* fat32_read_file(const char* filename, uint32_t* out_size) {
    char target_name[11];
    format_fat_name(filename, target_name);
    
    uint32_t current_cluster = root_cluster;
    uint8_t* cluster_buf = (uint8_t*)kmalloc(sectors_per_cluster * 512);
    
    // Ищем файл в корневом каталоге
    fat32_dir_entry_t* file_entry = NULL;
    
    while (current_cluster < 0x0FFFFFF8) {
        uint32_t lba = data_lba + (current_cluster - 2) * sectors_per_cluster;
        read_sectors_ata_pio((uint64_t)cluster_buf, lba, sectors_per_cluster);
        
        fat32_dir_entry_t* dirs = (fat32_dir_entry_t*)cluster_buf;
        int entries_count = (sectors_per_cluster * 512) / sizeof(fat32_dir_entry_t);
        
        for (int i = 0; i < entries_count; i++) {
            if (dirs[i].name[0] == 0x00) break; // Конец каталога
            if (dirs[i].name[0] == 0xE5) continue; // Удаленный файл
            if (dirs[i].attributes & 0x08) continue; // Метка тома
            
            // Сравниваем имена
            int match = 1;
            for (int j = 0; j < 11; j++) {
                if (dirs[i].name[j] != target_name[j]) { match = 0; break; }
            }
            
            if (match) {
                file_entry = &dirs[i];
                break;
            }
        }
        
        if (file_entry) break; // Файл найден!
        current_cluster = get_next_cluster(current_cluster);
    }
    
    if (!file_entry) {
        kfree(cluster_buf);
        return NULL; // Файл не найден
    }
    
    // Файл найден! Подготавливаем загрузку
    uint32_t start_cluster = (file_entry->cluster_high << 16) | file_entry->cluster_low;
    uint32_t file_size = file_entry->size;
    if (out_size) *out_size = file_size;
    
    uint8_t* file_data = (uint8_t*)kmalloc(file_size + 512); // +512 для запаса хвоста сектора
    uint8_t* write_ptr = file_data;
    uint32_t bytes_left = file_size;
    
    current_cluster = start_cluster;
    
    // Читаем кластеры файла один за другим
    while (current_cluster < 0x0FFFFFF8 && bytes_left > 0) {
        uint32_t lba = data_lba + (current_cluster - 2) * sectors_per_cluster;
        read_sectors_ata_pio((uint64_t)cluster_buf, lba, sectors_per_cluster);
        
        uint32_t bytes_to_copy = bytes_left > (sectors_per_cluster * 512) ? (sectors_per_cluster * 512) : bytes_left;
        memcpy(write_ptr, cluster_buf, bytes_to_copy);
        
        write_ptr += bytes_to_copy;
        bytes_left -= bytes_to_copy;
        current_cluster = get_next_cluster(current_cluster);
    }
    
    kfree(cluster_buf);
    return file_data;
}