#include "fat32.h"
#include "../drivers/disk/ata.h"
#include "../system/memory.h"
#include "../libc/string.h"
#include "../drivers/vga/vesa.h"

static uint32_t part_lba = 0;
static fat32_bpb_t bpb;
static uint32_t first_data_sector;
static uint32_t fat_start_sector;

void fat32_init() {
    // 1. Сначала узнаем, видит ли драйвер диск вообще
    ata_identify();

    uint8_t* sector_buf = kmalloc(512);
    if (!sector_buf) return;
    
    // Чистим буфер ПЕРЕД чтением, чтобы точно знать, что ATA туда что-то записала
    memset(sector_buf, 0xCC, 512); 
    
    read_sectors_ata_pio((uintptr_t)sector_buf, 0, 1);

    // Если в буфере всё еще 0xCC - значит ATA вообще ничего не прочитала (вернулась по ошибке)
    if (sector_buf[0] == 0xCC && sector_buf[1] == 0xCC) {
        term_print("FAT32 Error: ATA read failed (buffer untouched)!\n");
        kfree(sector_buf);
        return;
    }

    // Если там нули
    bool all_zeros = true;
    for(int i=0; i<512; i++) if(sector_buf[i] != 0) all_zeros = false;
    if(all_zeros) {
        term_print("FAT32 Error: Sector 0 is ALL ZEROS. Check hdd.img!\n");
        kfree(sector_buf);
        return;
    }

    // Дальше твоя логика поиска раздела...
    if (sector_buf[510] == 0x55 && sector_buf[511] == 0xAA) {
        // Проверяем на FAT32
        if (memcmp(&sector_buf[0x52], "FAT32", 5) == 0) {
             part_lba = 0;
             term_print("FAT32: Found Superfloppy.\n");
        } else {
             // Берем LBA первого раздела из MBR (смещение 446 + 8 байт)
             part_lba = *(uint32_t*)&sector_buf[454]; 
             term_print("FAT32: Partition found at LBA ");
             // Тут можно вывести part_lba
             read_sectors_ata_pio((uintptr_t)sector_buf, part_lba, 1);
        }
    } else {
        term_print("FAT32 Error: No MBR signature (55AA).\n");
        kfree(sector_buf);
        return;
    }

    memcpy(&bpb, sector_buf, sizeof(fat32_bpb_t));
    fat_start_sector = part_lba + bpb.reserved_sectors;
    first_data_sector = fat_start_sector + (bpb.fat_count * bpb.sectors_per_fat_32);
    
    kfree(sector_buf);
    term_print("FAT32: Mounted successfully!\n");
}

// Получаем следующий кластер из таблицы FAT
uint32_t fat32_get_next_cluster(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start_sector + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;

    uint8_t buf[512];
    read_sectors_ata_pio((uintptr_t)buf, fat_sector, 1);
    return (*(uint32_t*)&buf[ent_offset]) & 0x0FFFFFFF;
}

uint8_t* fat32_read_file(const char* name, uint32_t* out_size) {
    if (!name || name[0] == '\0') {
        term_print("FAT32: Invalid filename!\n");
        return 0;
    }

    // Проверка инициализации BPB
    if (bpb.sectors_per_cluster == 0) {
        term_print("FAT32: FS not initialized or invalid!\n");
        return 0;
    }

    char fat_name[11];
    memset(fat_name, ' ', 11);
    int i = 0, j = 0;
    while(name[i] && name[i] != '.' && j < 8) fat_name[j++] = name[i++];
    if(name[i] == '.') {
        i++; j = 8;
        while(name[i] && j < 11) fat_name[j++] = name[i++];
    }
    for(int k=0; k<11; k++) if(fat_name[k] >= 'a' && fat_name[k] <= 'z') fat_name[k] -= 32;

    uint32_t current_cluster = bpb.root_cluster;
    if (current_cluster < 2) {
        term_print("FAT32: Root cluster error!\n");
        return 0;
    }

    uint8_t* cluster_buf = kmalloc(bpb.sectors_per_cluster * 512);

    term_print("FAT32: Searching for file...\n");

    while (current_cluster >= 2 && current_cluster < 0x0FFFFFF8) {
        uint32_t lba = first_data_sector + (current_cluster - 2) * bpb.sectors_per_cluster;
        read_sectors_ata_pio((uintptr_t)cluster_buf, lba, bpb.sectors_per_cluster);

        fat32_entry_t* entries = (fat32_entry_t*)cluster_buf;
        for (int e = 0; e < (bpb.sectors_per_cluster * 512 / 32); e++) {
            if (entries[e].name[0] == 0) goto not_found;
            if (memcmp(entries[e].name, fat_name, 11) == 0) {
                // ФАЙЛ НАЙДЕН
                uint32_t size = entries[e].file_size;
                *out_size = size;
                uint8_t* file_data = kmalloc(size + 1024);
                
                uint32_t f_cluster = (entries[e].cluster_high << 16) | entries[e].cluster_low;
                uint32_t copied = 0;
                
                term_print("FAT32: Loading clusters...\n");
                while (f_cluster >= 2 && f_cluster < 0x0FFFFFF8 && copied < size) {
                    uint32_t f_lba = first_data_sector + (f_cluster - 2) * bpb.sectors_per_cluster;
                    read_sectors_ata_pio((uintptr_t)(file_data + copied), f_lba, bpb.sectors_per_cluster);
                    copied += (bpb.sectors_per_cluster * 512);
                    f_cluster = fat32_get_next_cluster(f_cluster);
                }
                kfree(cluster_buf);
                term_print("FAT32: Done.\n");
                return file_data;
            }
        }
        current_cluster = fat32_get_next_cluster(current_cluster);
        // Защита от бесконечного цикла, если FAT битая
        if (current_cluster == 0) break;
    }

not_found:
    kfree(cluster_buf);
    term_print("FAT32: File not found.\n");
    return 0;
}

void fat32_list_files() {
    if (bpb.sectors_per_cluster == 0) {
        term_print("FAT32: Not initialized!\n");
        return;
    }

    uint8_t* buf = kmalloc(bpb.sectors_per_cluster * 512);
    uint32_t cluster = bpb.root_cluster;

    term_print("--- FAT32 ROOT DIR ---\n");
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t lba = first_data_sector + (cluster - 2) * bpb.sectors_per_cluster;
        read_sectors_ata_pio((uintptr_t)buf, lba, bpb.sectors_per_cluster);

        fat32_entry_t* entries = (fat32_entry_t*)buf;
        for (int i = 0; i < (bpb.sectors_per_cluster * 512 / 32); i++) {
            if (entries[i].name[0] == 0) break; // Конец списка
            if (entries[i].name[0] == 0xE5) continue; // Удален
            if (entries[i].attr & 0x08) continue; // Метка тома (Volume ID)

            // Выводим имя файла (8.3 формат)
            char namebuf[13];
            int p = 0;
            for(int j=0; j<8; j++) if(entries[i].name[j] != ' ') namebuf[p++] = entries[i].name[j];
            namebuf[p++] = '.';
            for(int j=8; j<11; j++) if(entries[i].name[j] != ' ') namebuf[p++] = entries[i].name[j];
            namebuf[p] = '\0';

            term_print(namebuf);
            term_print("\n");
        }
        cluster = fat32_get_next_cluster(cluster);
    }
    kfree(buf);
}

int fat32_get_files(fat32_file_info_t* out_list, int max_files) {
    if (bpb.sectors_per_cluster == 0) return 0;

    uint8_t* buf = kmalloc(bpb.sectors_per_cluster * 512);
    uint32_t cluster = bpb.root_cluster;
    int count = 0;

    while (cluster >= 2 && cluster < 0x0FFFFFF8 && count < max_files) {
        uint32_t lba = first_data_sector + (cluster - 2) * bpb.sectors_per_cluster;
        read_sectors_ata_pio((uintptr_t)buf, lba, bpb.sectors_per_cluster);

        fat32_entry_t* entries = (fat32_entry_t*)buf;
        for (int i = 0; i < (bpb.sectors_per_cluster * 512 / 32); i++) {
            if (entries[i].name[0] == 0) break; 
            if (entries[i].name[0] == 0xE5 || (entries[i].attr & 0x08)) continue;

            // Форматируем имя 8.3 -> "NAME.EXT"
            int p = 0;
            for(int j=0; j<8; j++) if(entries[i].name[j] != ' ') out_list[count].name[p++] = entries[i].name[j];
            out_list[count].name[p++] = '.';
            for(int j=8; j<11; j++) if(entries[i].name[j] != ' ') out_list[count].name[p++] = entries[i].name[j];
            out_list[count].name[p] = '\0';
            
            out_list[count].size = entries[i].file_size;
            out_list[count].cluster = (entries[i].cluster_high << 16) | entries[i].cluster_low;
            count++;
            if (count >= max_files) break;
        }
        cluster = fat32_get_next_cluster(cluster);
    }
    kfree(buf);
    return count;
}

uint32_t fat32_find_free_cluster() {
    uint32_t fat_sectors = bpb.sectors_per_fat_32;
    uint8_t* buf = kmalloc(512);

    for (uint32_t i = 0; i < fat_sectors; i++) {
        read_sectors_ata_pio((uintptr_t)buf, fat_start_sector + i, 1);
        uint32_t* entries = (uint32_t*)buf;
        for (int j = 0; j < 128; j++) {
            if ((entries[j] & 0x0FFFFFFF) == 0) {
                kfree(buf);
                return (i * 128) + j;
            }
        }
    }
    kfree(buf);
    return 0; // Нет места
}

void fat32_set_cluster_entry(uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start_sector + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;

    uint8_t buf[512];
    read_sectors_ata_pio((uintptr_t)buf, fat_sector, 1);
    *(uint32_t*)&buf[ent_offset] = (value & 0x0FFFFFFF);
    write_sectors_ata_pio(fat_sector, 1, (uint16_t*)buf);
}

void fat32_save_file(const char* name, const char* data, uint32_t size) {
    // 1. Подготовка имени (8.3)
    char fat_name[11];
    memset(fat_name, ' ', 11);
    int p = 0;
    while(name[p] && name[p] != '.' && p < 8) { fat_name[p] = name[p]; p++; }
    // Для простоты считаем, что расширение .TXT
    memcpy(&fat_name[8], "TXT", 3); 

    // 2. Ищем свободный кластер для данных
    uint32_t cluster = fat32_find_free_cluster();
    if (cluster < 2) return;

    // 3. Записываем данные в кластер
    uint32_t lba = first_data_sector + (cluster - 2) * bpb.sectors_per_cluster;
    uint8_t* data_buf = kmalloc(bpb.sectors_per_cluster * 512);
    memset(data_buf, 0, bpb.sectors_per_cluster * 512);
    memcpy(data_buf, data, size > 512 ? 512 : size); // Пока пишем только 1 сектор для теста
    write_sectors_ata_pio(lba, 1, (uint16_t*)data_buf);
    kfree(data_buf);

    // 4. Обновляем FAT
    fat32_set_cluster_entry(cluster, 0x0FFFFFFF);

    // 5. Ищем свободное место в корневой директории
    uint8_t* root_buf = kmalloc(512);
    read_sectors_ata_pio((uintptr_t)root_buf, first_data_sector + (bpb.root_cluster - 2) * bpb.sectors_per_cluster, 1);
    
    fat32_entry_t* entries = (fat32_entry_t*)root_buf;
    for (int i = 0; i < 16; i++) {
        if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
            memcpy(entries[i].name, fat_name, 11);
            entries[i].attr = 0x20; // Archive
            entries[i].file_size = size;
            entries[i].cluster_low = cluster & 0xFFFF;
            entries[i].cluster_high = (cluster >> 16) & 0xFFFF;
            
            write_sectors_ata_pio(first_data_sector + (bpb.root_cluster - 2) * bpb.sectors_per_cluster, 1, (uint16_t*)root_buf);
            break;
        }
    }
    kfree(root_buf);
    term_print("FAT32: File saved successfully.\n");
}