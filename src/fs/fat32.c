#include "fat32.h"
#include "vfs.h"
#include "../drivers/disk/ata.h"
#include "../system/memory.h"
#include "../libc/string.h"
#include "../drivers/vga/vesa.h"

extern void term_print(const char* str);

static vfs_dirent_t fat_shared_dirent;
static uint8_t fat_cache[512] __attribute__((aligned(16)));
static uint32_t cached_fat_sector = 0xFFFFFFFF;
static uint32_t part_lba = 0;
static fat32_bpb_t* bpb = NULL;
static uint32_t first_data_sector;
static uint32_t fat_start_sector;
static bool fat32_ready = false;

uint32_t fat32_vfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!fat32_ready) return 0;
    
    uint32_t fsize, fcluster;
    if (!fat32_find_file_info(node->name, &fsize, &fcluster)) return 0;
    
    if (offset >= fsize) return 0;
    if (offset + size > fsize) size = fsize - offset;
    
    uint32_t cluster_size = bpb->sectors_per_cluster * 512;
    uint32_t clusters_to_skip = offset / cluster_size;
    uint32_t internal_offset = offset % cluster_size;
    
    uint32_t curr = fcluster;
    for (uint32_t i = 0; i < clusters_to_skip; i++) {
        curr = fat32_get_next_cluster(curr);
        if (curr < 2 || curr >= 0x0FFFFFF8) return 0;
    }
    
    uint8_t* cbuf = kmalloc(cluster_size);
    uint32_t read = 0;
    while (read < size) {
        uint32_t lba = first_data_sector + (curr - 2) * bpb->sectors_per_cluster;
        read_sectors_ata_pio((uintptr_t)cbuf, lba, bpb->sectors_per_cluster);
        
        uint32_t to_copy = cluster_size - internal_offset;
        if (to_copy > size - read) to_copy = size - read;
        
        memcpy(buffer + read, cbuf + internal_offset, to_copy);
        read += to_copy;
        internal_offset = 0; 
        
        if (read < size) {
            curr = fat32_get_next_cluster(curr);
            if (curr < 2 || curr >= 0x0FFFFFF8) break;
        }
    }
    kfree(cbuf);
    return read;
}

uint32_t fat32_vfs_write(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!fat32_ready) return 0;
    // Bridging to existing save_file (which handles allocation/overwriting)
    fat32_save_file(node->name, (char*)buffer, size);
    return size;
}

struct vfs_dirent* fat32_vfs_readdir(vfs_node_t* node, uint32_t index) {
    if (bpb->sectors_per_cluster == 0) return NULL;

    uint8_t* buf = kmalloc(bpb->sectors_per_cluster * 512);
    uint32_t cluster = bpb->root_cluster;
    uint32_t current_idx = 0;

    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t lba = first_data_sector + (cluster - 2) * bpb->sectors_per_cluster;
        read_sectors_ata_pio((uintptr_t)buf, lba, bpb->sectors_per_cluster);

        fat32_entry_t* entries = (fat32_entry_t*)buf;
        for (int i = 0; i < (int)(bpb->sectors_per_cluster * 512 / 32); i++) {
            if (entries[i].name[0] == 0x00) break;
            if ((uint8_t)entries[i].name[0] == 0xE5) continue; 
            if (entries[i].attr & 0x08) continue; // Volume Label

            if (current_idx == index) {
                // Format name
                int p = 0;
                for(int j=0; j<8; j++) {
                    if(entries[i].name[j] != ' ') fat_shared_dirent.name[p++] = entries[i].name[j];
                }
                if (entries[i].name[8] != ' ') {
                    fat_shared_dirent.name[p++] = '.';
                    for(int j=8; j<11; j++) {
                        if(entries[i].name[j] != ' ') fat_shared_dirent.name[p++] = entries[i].name[j];
                    }
                }
                fat_shared_dirent.name[p] = '\0';
                fat_shared_dirent.size = entries[i].file_size;
                fat_shared_dirent.inode = (entries[i].cluster_high << 16) | entries[i].cluster_low;
                
                kfree(buf);
                return &fat_shared_dirent;
            }
            current_idx++;
        }
        cluster = fat32_get_next_cluster(cluster);
    }
    kfree(buf);
    return NULL;
}

vfs_node_t* fat32_get_root_node() {
    if (!fat32_ready) return NULL;
    vfs_node_t* node = kmalloc(sizeof(vfs_node_t));
    memset(node, 0, sizeof(vfs_node_t));
    strcpy(node->name, "FAT32_DISK");
    node->flags = 0x01; // Directory
    node->read = fat32_vfs_read;
    node->readdir = fat32_vfs_readdir;
    node->write = fat32_vfs_write;
    return node;
}

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
    
    fat32_ready = true;
    kfree(sector_buf);
    term_print("FAT32: Mounted successfully!\n");
}
// Получаем следующий кластер из таблицы FAT
uint32_t fat32_get_next_cluster(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start_sector + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;

    if (cached_fat_sector != fat_sector) {
        read_sectors_ata_pio((uintptr_t)fat_cache, fat_sector, 1);
        cached_fat_sector = fat_sector;
    }
    
    uint32_t next = (*(uint32_t*)&fat_cache[ent_offset]) & 0x0FFFFFFF;
    return next;
}

uint8_t* fat32_read_file(const char* name, uint32_t* out_size) {
    char fat_name[11];
    fat32_to_83(name, fat_name);

    uint32_t current_cluster = bpb->root_cluster;
    uint32_t cluster_size = bpb->sectors_per_cluster * 512; // ОБЪЯВЛЯЕМ ТУТ
    uint8_t* cluster_buf = kmalloc(cluster_size);

    while (current_cluster >= 2 && current_cluster < 0x0FFFFFF8) {
        uint32_t lba = first_data_sector + (current_cluster - 2) * bpb->sectors_per_cluster;
        read_sectors_ata_pio((uintptr_t)cluster_buf, lba, bpb->sectors_per_cluster);

        fat32_entry_t* entries = (fat32_entry_t*)cluster_buf;
        for (int e = 0; e < (int)(cluster_size / 32); e++) {
            if (entries[e].name[0] == 0x00) break;
            
            // ФИКС ВОРНИНГА: кастим к uint8_t
            if ((uint8_t)entries[e].name[0] == 0xE5) continue; 
            if (entries[e].attr == 0x0F) continue; 

            if (memcmp(entries[e].name, fat_name, 11) == 0) {
                uint32_t size = entries[e].file_size;
                *out_size = size;
                
                uint8_t* file_data = kmalloc(size + 512); 
                uint32_t f_cluster = (entries[e].cluster_high << 16) | entries[e].cluster_low;
                uint32_t copied = 0;
                
                while (f_cluster >= 2 && f_cluster < 0x0FFFFFF8 && copied < size) {
                    uint32_t f_lba = first_data_sector + (f_cluster - 2) * bpb->sectors_per_cluster;
                    
                    // ФИКС ОШИБКИ: теперь cluster_size определен
                    uint32_t remaining = size - copied;
                    uint32_t to_read_now = (remaining > cluster_size) ? cluster_size : remaining;

                    if (to_read_now == cluster_size) {
                        read_sectors_ata_pio((uintptr_t)(file_data + copied), f_lba, bpb->sectors_per_cluster);
                    } else {
                        // Для последнего куска используем временный буфер на куче
                        uint8_t* tmp_cluster = kmalloc(cluster_size);
                        read_sectors_ata_pio((uintptr_t)tmp_cluster, f_lba, bpb->sectors_per_cluster);
                        memcpy(file_data + copied, tmp_cluster, to_read_now);
                        kfree(tmp_cluster);
                    }

                    copied += to_read_now;
                    f_cluster = fat32_get_next_cluster(f_cluster);
                }
                kfree(cluster_buf);
                return file_data;
            }
        }
        current_cluster = fat32_get_next_cluster(current_cluster);
    }

    kfree(cluster_buf);
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

uint32_t fat32_allocate_cluster_chain(uint32_t count) {
    if (count == 0) return 0;

    uint32_t first_cluster = 0;
    uint32_t last_cluster = 0;
    uint32_t found = 0;

    // Перебираем всю таблицу FAT
    // Для оптимизации можно хранить индекс последнего найденного, но пока так
    for (uint32_t c = 2; found < count; c++) {
        if (fat32_get_next_cluster(c) == 0) { // Если кластер свободен
            if (first_cluster == 0) {
                first_cluster = c;
            } else {
                // Связываем предыдущий найденный кластер с текущим
                fat32_set_cluster_entry(last_cluster, c);
            }
            last_cluster = c;
            found++;
            
            // Сразу помечаем как конец цепочки (потом перезапишем, если будет продолжение)
            fat32_set_cluster_entry(c, 0x0FFFFFFF);
        }
        
        // Тут стоило бы добавить проверку на выход за пределы таблицы FAT
    }

    return first_cluster;
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

    uint32_t cluster_size = bpb->sectors_per_cluster * 512;
    uint32_t current_root_cluster = bpb->root_cluster;
    
    int target_idx = -1;
    uint32_t target_cluster_lba = 0; // LBA кластера корня, где лежит запись
    uint8_t* root_buf = (uint8_t*)kmalloc(cluster_size);

    // --- ШАГ 1: Поиск места в корне ---
    while (current_root_cluster >= 2 && current_root_cluster < 0x0FFFFFF8) {
        uint32_t lba = first_data_sector + (current_root_cluster - 2) * bpb->sectors_per_cluster;
        read_sectors_ata_pio((uintptr_t)root_buf, lba, bpb->sectors_per_cluster);
        
        fat32_entry_t* entries = (fat32_entry_t*)root_buf;
        for (int i = 0; i < (int)(cluster_size / 32); i++) {
            if (memcmp(entries[i].name, fat_name, 11) == 0) {
                target_idx = i;
                target_cluster_lba = lba;
                goto found_slot;
            }
            if (target_idx == -1 && (entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5)) {
                target_idx = i;
                target_cluster_lba = lba;
            }
        }
        current_root_cluster = fat32_get_next_cluster(current_root_cluster);
    }

found_slot:
    if (target_idx == -1) {
        term_print("FAT32: Root dir full!\n");
        kfree(root_buf);
        return;
    }

    // --- ШАГ 2: Выделение места под файл ---
    uint32_t clusters_needed = (size + cluster_size - 1) / cluster_size;
    if (clusters_needed == 0) clusters_needed = 1;
    
    // Вызываем аллокатор (который мы написали раньше)
    uint32_t first_file_cluster = fat32_allocate_cluster_chain(clusters_needed);
    if (first_file_cluster == 0) {
        term_print("FAT32: No free clusters!\n");
        kfree(root_buf);
        return;
    }

    // --- ШАГ 3: Обновление записи в директории ---
    // Перечитываем нужный сектор корня (на случай если мы ушли далеко по цепочке)
    read_sectors_ata_pio((uintptr_t)root_buf, target_cluster_lba, bpb->sectors_per_cluster);
    fat32_entry_t* entries = (fat32_entry_t*)root_buf;

    memcpy(entries[target_idx].name, fat_name, 11);
    entries[target_idx].attr = 0x20;
    entries[target_idx].file_size = size;
    entries[target_idx].cluster_low = first_file_cluster & 0xFFFF;
    entries[target_idx].cluster_high = (first_file_cluster >> 16) & 0xFFFF;

    write_sectors_ata_pio(target_cluster_lba, bpb->sectors_per_cluster, (uint16_t*)root_buf);

    // --- ШАГ 4: Запись данных файла ---
    uint32_t cluster_to_write = first_file_cluster;
    uint32_t bytes_left = size;
    uint8_t* data_ptr = (uint8_t*)data;
    uint8_t* write_buf = (uint8_t*)kmalloc(cluster_size);

    while (bytes_left > 0 && cluster_to_write < 0x0FFFFFF8) {
        uint32_t lba = first_data_sector + (cluster_to_write - 2) * bpb->sectors_per_cluster;
        
        memset(write_buf, 0, cluster_size);
        uint32_t to_copy = (bytes_left > cluster_size) ? cluster_size : bytes_left;
        memcpy(write_buf, data_ptr, to_copy);
        
        write_sectors_ata_pio(lba, bpb->sectors_per_cluster, (uint16_t*)write_buf);
        
        data_ptr += to_copy;
        bytes_left -= to_copy;
        
        if (bytes_left > 0) {
            cluster_to_write = fat32_get_next_cluster(cluster_to_write);
            if (cluster_to_write >= 0x0FFFFFF8 && bytes_left > 0) {
                term_print("FAT32: Chain error during write!\n");
                break;
            }
        }
    }

    kfree(write_buf);
    kfree(root_buf);
    term_print("FAT32: File saved successfully!\n");
}

void fat32_to_83(const char* src, char* dst) {
    memset(dst, ' ', 11); // Заполняем пробелами по умолчанию
    int i = 0, dst_idx = 0;

    // 1. Копируем имя (до точки или до 8 символов)
    while (src[i] != '\0' && src[i] != '.' && dst_idx < 8) {
        char c = src[i];
        // Пропускаем мусор (если он случайно пролез)
        if (c != '\r' && c != '\n' && c != ' ') {
            if (c >= 'a' && c <= 'z') c -= 32; // To uppercase
            dst[dst_idx++] = c;
        }
        i++;
    }

    // 2. Ищем точку вручную (без strstr)
    int dot_idx = -1;
    for (int k = 0; src[k] != '\0'; k++) {
        if (src[k] == '.') {
            dot_idx = k;
            break;
        }
    }

    // 3. Если нашли точку, копируем расширение (до 3 символов)
    if (dot_idx != -1) {
        int ext_start = dot_idx + 1;
        dst_idx = 8; // Расширение всегда начинается с 8-го байта
        for (int j = 0; j < 3 && src[ext_start + j] != '\0'; j++) {
            char c = src[ext_start + j];
            if (c != '\r' && c != '\n' && c != ' ') {
                if (c >= 'a' && c <= 'z') c -= 32;
                dst[dst_idx++] = c;
            }
        }
    }
}

// В fat32.c

uint32_t fat32_get_cluster_size() {
    return bpb->sectors_per_cluster * 512;
}

uint32_t fat32_get_first_data_sector() {
    return first_data_sector;
}

uint32_t fat32_get_sectors_per_cluster() {
    return bpb->sectors_per_cluster;
}

// Новая функция: находит информацию о файле без его чтения
bool fat32_find_file_info(const char* name, uint32_t* out_size, uint32_t* out_cluster) {
    char fat_name[11];
    fat32_to_83(name, fat_name);

    uint32_t current_cluster = bpb->root_cluster;
    uint32_t cluster_size = bpb->sectors_per_cluster * 512;
    uint8_t* cluster_buf = kmalloc(cluster_size);

    while (current_cluster >= 2 && current_cluster < 0x0FFFFFF8) {
        uint32_t lba = first_data_sector + (current_cluster - 2) * bpb->sectors_per_cluster;
        read_sectors_ata_pio((uintptr_t)cluster_buf, lba, bpb->sectors_per_cluster);

        fat32_entry_t* entries = (fat32_entry_t*)cluster_buf;
        for (int e = 0; e < (int)(cluster_size / 32); e++) {
            if (entries[e].name[0] == 0x00) break;
            if ((uint8_t)entries[e].name[0] == 0xE5) continue;
            if (entries[e].attr == 0x0F) continue;

            if (memcmp(entries[e].name, fat_name, 11) == 0) {
                *out_size = entries[e].file_size;
                *out_cluster = (entries[e].cluster_high << 16) | entries[e].cluster_low;
                kfree(cluster_buf);
                return true;
            }
        }
        current_cluster = fat32_get_next_cluster(current_cluster);
    }

    kfree(cluster_buf);
    return false;
}