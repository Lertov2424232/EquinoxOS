#include "fat32.h"
#include "drivers/disk/ata.h"
#include "system/memory.h"
#include "libc/string.h"

static uint32_t part_lba = 0;
static fat32_bpb_t bpb;
static uint32_t first_data_sector;
static uint32_t fat_start_sector;

void fat32_init() {
    uint8_t* sector_buf = kmalloc(512);
    memset(sector_buf, 0, 512);
    
    // Пытаемся прочитать сектор 0
    read_sectors_ata_pio((uintptr_t)sector_buf, 0, 1);

    // ВЫВОДИМ ДЕБАГ НА ЭКРАН (прямо в VESA)
    vesa_draw_string("Disk Debug:", 10, 300, 0xFFFF00);
    vesa_draw_string_hex("Sig (should be 55AA): ", 10, 320, *(uint16_t*)&sector_buf[510], 0x00FF00);
    vesa_draw_string_hex("Byte 0 (should be EB or E9): ", 10, 340, sector_buf[0], 0x00FF00);

    if (sector_buf[510] != 0x55 || sector_buf[511] != 0xAA) {
        term_print("FAT32: MBR Signature missing!\n");
        kfree(sector_buf);
        return;
    }

    // Проверяем: это MBR или сразу BPB?
    // В FAT32 по смещению 0x52 обычно строка "FAT32   "
    if (memcmp(&sector_buf[0x52], "FAT32", 5) == 0) {
        term_print("FAT32: Superfloppy (No MBR) detected.\n");
        part_lba = 0;
    } else {
        // Читаем смещение первого раздела из MBR
        part_lba = *(uint32_t*)&sector_buf[446 + 8]; 
        vesa_draw_string_hex("Partition LBA: ", 10, 360, part_lba, 0x00FF00);
        
        // Перечитываем первый сектор раздела
        read_sectors_ata_pio((uintptr_t)sector_buf, part_lba, 1);
    }

    memcpy(&bpb, sector_buf, sizeof(fat32_bpb_t));
    
    // Если и тут 0, значит мы прочитали не BPB
    vesa_draw_string_hex("Sectors per Cluster: ", 10, 380, bpb.sectors_per_cluster, 0x00FF00);

    fat_start_sector = part_lba + bpb.reserved_sectors;
    first_data_sector = fat_start_sector + (bpb.fat_count * bpb.sectors_per_fat_32);
    
    kfree(sector_buf);
    term_print("FAT32: System Ready.\n");
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