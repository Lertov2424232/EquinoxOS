#include "fat32.h"
#include "../drivers/disk/ata.h"
#include "../system/memory.h"
#include "../libc/string.h"
#include "../drivers/vga/vesa.h"

static uint32_t part_lba = 0;
static fat32_bpb_t* bpb = NULL;
static uint32_t first_data_sector;
static uint32_t fat_start_sector;

void fat32_init() {
    ata_identify();
    
    // Выделяем память в куче, здесь писать МОЖНО
    if (!bpb) bpb = (fat32_bpb_t*)kmalloc(sizeof(fat32_bpb_t));

    uint8_t* sector_buf = kmalloc(512);
    read_sectors_ata_pio((uintptr_t)sector_buf, 0, 1);
    
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

    memcpy(bpb, sector_buf, sizeof(fat32_bpb_t));
    
    // ВАЖНО: везде в коде замени bpb-> на bpb->
    fat_start_sector = part_lba + bpb->reserved_sectors;
    first_data_sector = fat_start_sector + (bpb->fat_count * bpb->sectors_per_fat_32);
    
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
    char fat_name[11];
    fat32_to_83(name, fat_name); // Превращаем "NOTES.TXT" в "NOTES   TXT"

    uint32_t current_cluster = bpb->root_cluster;
    uint8_t* cluster_buf = kmalloc(bpb->sectors_per_cluster * 512);

    while (current_cluster >= 2 && current_cluster < 0x0FFFFFF8) {
        uint32_t lba = first_data_sector + (current_cluster - 2) * bpb->sectors_per_cluster;
        read_sectors_ata_pio((uintptr_t)cluster_buf, lba, bpb->sectors_per_cluster);

        fat32_entry_t* entries = (fat32_entry_t*)cluster_buf;
        for (int e = 0; e < (int)(bpb->sectors_per_cluster * 512 / 32); e++) {
            if (entries[e].name[0] == 0x00) break;    // Конец списка
            if (entries[e].name[0] == 0xE5) continue; // Файл удален - ИГНОРИРУЕМ

            // СРАВНИВАЕМ С ПОДГОТОВЛЕННЫМ ИМЕНЕМ!
            if (memcmp(entries[e].name, fat_name, 11) == 0) {
                uint32_t size = entries[e].file_size;
                *out_size = size;
                
                uint8_t* file_data = kmalloc(size + 512); // Запас под сектор
                uint32_t f_cluster = (entries[e].cluster_high << 16) | entries[e].cluster_low;
                uint32_t copied = 0;
                
                while (f_cluster >= 2 && f_cluster < 0x0FFFFFF8 && copied < size) {
                    uint32_t f_lba = first_data_sector + (f_cluster - 2) * bpb->sectors_per_cluster;
                    // Читаем целый кластер
                    read_sectors_ata_pio((uintptr_t)(file_data + copied), f_lba, bpb->sectors_per_cluster);
                    copied += (bpb->sectors_per_cluster * 512);
                    f_cluster = fat32_get_next_cluster(f_cluster);
                }
                kfree(cluster_buf);
                return file_data;
            }
        }
        current_cluster = fat32_get_next_cluster(current_cluster);
    }

    kfree(cluster_buf);
    term_print("FAT32: File not found during read.\n");
    return NULL;
}

void fat32_list_files() {
    if (bpb->sectors_per_cluster == 0) {
        term_print("FAT32: Not initialized!\n");
        return;
    }

    uint8_t* buf = kmalloc(bpb->sectors_per_cluster * 512);
    uint32_t cluster = bpb->root_cluster;

    term_print("--- FAT32 ROOT DIR ---\n");
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t lba = first_data_sector + (cluster - 2) * bpb->sectors_per_cluster;
        read_sectors_ata_pio((uintptr_t)buf, lba, bpb->sectors_per_cluster);

        fat32_entry_t* entries = (fat32_entry_t*)buf;
        for (int i = 0; i < (bpb->sectors_per_cluster * 512 / 32); i++) {
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
    uint8_t* buf = kmalloc(bpb->sectors_per_cluster * 512);
    uint32_t cluster = bpb->root_cluster;
    int count = 0;

    while (cluster >= 2 && cluster < 0x0FFFFFF8 && count < max_files) {
        uint32_t lba = first_data_sector + (cluster - 2) * bpb->sectors_per_cluster;
        read_sectors_ata_pio((uintptr_t)buf, lba, bpb->sectors_per_cluster);

        fat32_entry_t* entries = (fat32_entry_t*)buf;
        for (int i = 0; i < (int)(bpb->sectors_per_cluster * 512 / 32); i++) {
            if (entries[i].name[0] == 0x00) break;
            
            // ЖЕСТКАЯ ПРОВЕРКА НА УДАЛЕНИЕ И СИСТЕМНЫЕ ФАЙЛЫ
            if ((uint8_t)entries[i].name[0] == 0xE5) continue; 
            if (entries[i].attr & 0x08) continue; // Volume Label

            // Форматируем красиво для Explorer: "NOTES.TXT"
            char* out_name = out_list[count].name;
            int p = 0;
            for(int j=0; j<8; j++) {
                if(entries[i].name[j] != ' ') out_name[p++] = entries[i].name[j];
            }
            // Добавляем точку, только если есть расширение
            if (entries[i].name[8] != ' ') {
                out_name[p++] = '.';
                for(int j=8; j<11; j++) {
                    if(entries[i].name[j] != ' ') out_name[p++] = entries[i].name[j];
                }
            }
            out_name[p] = '\0';
            
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
    uint32_t fat_sectors = bpb->sectors_per_fat_32;
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
    char fat_name[11];
    fat32_to_83(name, fat_name);

    uint8_t* root_buf = kmalloc(bpb->sectors_per_cluster * 512);
    uint32_t root_lba = first_data_sector + (bpb->root_cluster - 2) * bpb->sectors_per_cluster;
    read_sectors_ata_pio((uintptr_t)root_buf, root_lba, bpb->sectors_per_cluster);
    
    fat32_entry_t* entries = (fat32_entry_t*)root_buf;
    int target_idx = -1;

    // Сначала ищем: может такой файл уже есть?
    for (int i = 0; i < (bpb->sectors_per_cluster * 512 / 32); i++) {
        if (memcmp(entries[i].name, fat_name, 11) == 0) {
            target_idx = i;
            break;
        }
    }

    // Если не нашли, ищем свободное место (как раньше)
    if (target_idx == -1) {
        for (int i = 0; i < (bpb->sectors_per_cluster * 512 / 32); i++) {
            if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
                target_idx = i;
                memcpy(entries[target_idx].name, fat_name, 11);
                // Выделяем новый кластер только для нового файла
                uint32_t new_cluster = fat32_find_free_cluster();
                entries[target_idx].cluster_low = new_cluster & 0xFFFF;
                entries[target_idx].cluster_high = (new_cluster >> 16) & 0xFFFF;
                fat32_set_cluster_entry(new_cluster, 0x0FFFFFFF);
                break;
            }
        }
    }

    if (target_idx != -1) {
        entries[target_idx].attr = 0x20;
        entries[target_idx].file_size = size;
        
        // Пишем данные в кластер файла
        uint32_t cluster = (entries[target_idx].cluster_high << 16) | entries[target_idx].cluster_low;
        uint32_t lba = first_data_sector + (cluster - 2) * bpb->sectors_per_cluster;
        
        uint8_t* data_buf = kmalloc(bpb->sectors_per_cluster * 512);
        memset(data_buf, 0, bpb->sectors_per_cluster * 512);
        memcpy(data_buf, data, size);
        write_sectors_ata_pio(lba, 1, (uint16_t*)data_buf);
        kfree(data_buf);

        // Сохраняем обновленную таблицу директории
        write_sectors_ata_pio(root_lba, 1, (uint16_t*)root_buf);
        term_print("FAT32: File updated/created.\n");
    }

    kfree(root_buf);
}

void fat32_to_83(const char* src, char* dst) {
    memset(dst, ' ', 11);
    int i = 0, j = 0;

    // Копируем имя (до точки или до конца)
    for (i = 0; i < 8 && src[i] != '.' && src[i] != '\0'; i++) {
        char c = src[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        dst[i] = c;
    }

    // Ищем точку для расширения
    const char* dot = strstr(src, ".");
    if (dot) {
        dot++; // Пропускаем саму точку
        for (j = 0; j < 3 && dot[j] != '\0'; j++) {
            char c = dot[j];
            if (c >= 'a' && c <= 'z') c -= 32;
            dst[8 + j] = c;
        }
    }
}