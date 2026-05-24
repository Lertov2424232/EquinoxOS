#include "fs.h"
#include "../drivers/hardware/disk/ata.h"
#include "../mem/memory.h"
#include "../../syslibc/string.h"

extern void term_print(const char* str);

// --- НАСТРОЙКИ ФАЙЛОВОЙ СИСТЕМЫ ---
#define FS_ROOT_DIR_LBA 1     // Сектор, где хранится таблица файлов
#define FS_DATA_START_LBA 10  // Сектор, с которого начинаются сами данные файлов

// =========================================================================

void init_fs() {
    uint8_t* zero_sector = (uint8_t*)kmalloc(512);
    if (!zero_sector) return; // Защита от нехватки памяти

    memset(zero_sector, 0, 512); // Быстрая очистка нулями
    write_sectors_ata_pio((uintptr_t)zero_sector, FS_ROOT_DIR_LBA, 1);
    
    kfree(zero_sector); // ИСПРАВЛЕНИЕ: Возвращаем память ОС!
    term_print("FS Initialized (Formatted).\n");
}

void list_files() {
    file_entry_t* dir = (file_entry_t*)kmalloc(512);
    if (!dir) return;

    read_sectors_ata_pio((uint64_t)dir, FS_ROOT_DIR_LBA, 1); 

    int found = 0;
    term_print("--- Files on disk ---\n");

    for (int i = 0; i < MAX_FILES; i++) {
        // Проверяем, что первый символ не 0 и не 0xFF (пустой сектор ATA)
        if (dir[i].name[0] != '\0' && (uint8_t)dir[i].name[0] != 0xFF) { 
            term_print(dir[i].name);
            found = 1;
        }
    }

    if (!found) {
        term_print("Disk is empty.\n");
    }
    
    kfree(dir); // ИСПРАВЛЕНИЕ: Не забываем освобождать
}

void create_file(char* name, char* content) {
    file_entry_t* dir = (file_entry_t*)kmalloc(512);
    if (!dir) return;
    
    read_sectors_ata_pio((uint64_t)dir, FS_ROOT_DIR_LBA, 1);

    for (int i = 0; i < MAX_FILES; i++) {
        if (dir[i].name[0] == '\0' || (uint8_t)dir[i].name[0] == 0xFF) {
            
            // 1. Готовим структуру файла
            memset(dir[i].name, 0, 16);
            for(int j = 0; j < 15 && name[j] != '\0'; j++) {
                dir[i].name[j] = name[j];
            }
            dir[i].size = 512;
            dir[i].start_lba = FS_DATA_START_LBA + i; 

            // 2. Готовим данные для записи
            uint8_t* data = (uint8_t*)kmalloc(512);
            if (!data) { kfree(dir); return; }
            
            memset(data, 0, 512);
            for(int j = 0; j < 511 && content[j] != '\0'; j++) {
                data[j] = content[j];
            }
            
            // 3. Пишем на диск
            write_sectors_ata_pio((uintptr_t)data, dir[i].start_lba, 1);
            write_sectors_ata_pio((uintptr_t)dir, FS_ROOT_DIR_LBA, 1);
            
            // 4. Очищаем за собой память
            kfree(data);
            kfree(dir);
            
            term_print("File created.\n");
            return;
        }
    }
    
    kfree(dir);
    term_print("Error: Disk full!\n");
}

void read_file(char* name) {
    file_entry_t* dir = (file_entry_t*)kmalloc(512);
    if (!dir) return;
    
    read_sectors_ata_pio((uint64_t)dir, FS_ROOT_DIR_LBA, 1);

    for (int i = 0; i < MAX_FILES; i++) {
        // Проверка совпадения имени
        int match = 1;
        for(int j = 0; j < 16; j++) {
            if (dir[i].name[j] != name[j]) { match = 0; break; }
            if (name[j] == '\0') break;
        }

        if (match && dir[i].name[0] != '\0') {
            char* data = (char*)kmalloc(512);
            if (!data) { kfree(dir); return; }
            
            read_sectors_ata_pio((uint64_t)data, dir[i].start_lba, 1);
            
            data[63] = '\0'; // Ограничим длину вывода (чтобы влезло в экран терминала)
            term_print("--- File content ---\n");
            term_print(data);
            
            kfree(data);
            kfree(dir);
            return;
        }
    }
    
    kfree(dir);
    term_print("File not found.\n");
}