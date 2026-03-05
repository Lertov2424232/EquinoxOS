#include "fs.h"
#include "../drivers/disk/ata.h"
#include "../system/memory.h"

// Объявляем внешнюю функцию для вывода
extern void term_print(const char* str);

// Функция для форматирования диска (забиваем директорию нулями)
void init_fs() {
    uint8_t* zero_sector = (uint8_t*)kmalloc(512);
    for(int i=0; i<512; i++) zero_sector[i] = 0;
    write_sectors_ata_pio(1, 1, (uint16_t*)zero_sector);
    term_print("FS Initialized (Formatted).");
}

void list_files() {
    file_entry_t* dir = (file_entry_t*)kmalloc(512);
    if (!dir) return;

    read_sectors_ata_pio((uint64_t)dir, 1, 1); 

    int found = 0;
    term_print("--- Files on disk ---");

    for (int i = 0; i < MAX_FILES; i++) {
        if (dir[i].name[0] != 0 && dir[i].name[0] != 0xFF) { // 0xFF бывает на пустых дисках
            term_print(dir[i].name);
            found = 1;
        }
    }

    if (!found) {
        term_print("Disk is empty.");
    }
}

void create_file(char* name, char* content) {
    file_entry_t* dir = (file_entry_t*)kmalloc(512);
    if (!dir) return;
    
    // Читаем директорию (Сектор 1)
    read_sectors_ata_pio((uint64_t)dir, 1, 1);

    for (int i = 0; i < MAX_FILES; i++) {
        if (dir[i].name[0] == 0 || dir[i].name[0] == 0xFF) {
            // Копируем имя
            for(int j=0; j<19 && name[j] != '\0'; j++) {
                dir[i].name[j] = name[j];
            }
            dir[i].name[19] = '\0'; // Гарантированный конец строки
            dir[i].size = 512;
            dir[i].start_lba = 10 + i; // Файлы храним начиная с 10 сектора

            // Готовим данные
            uint8_t* data = (uint8_t*)kmalloc(512);
            for(int j=0; j<512; j++) data[j] = 0;
            
            for(int j=0; j < 511 && content[j] != '\0'; j++) {
                data[j] = content[j];
            }
            
            // Пишем данные файла
            write_sectors_ata_pio(dir[i].start_lba, 1, (uint16_t*)data);
            // Обновляем директорию
            write_sectors_ata_pio(1, 1, (uint16_t*)dir);
            
            term_print("File created.");
            return;
        }
    }
    term_print("Error: Disk full!");
}

void read_file(char* name) {
    file_entry_t* dir = (file_entry_t*)kmalloc(512);
    read_sectors_ata_pio((uint64_t)dir, 1, 1);

    for (int i = 0; i < MAX_FILES; i++) {
        // Простая проверка имени (до первого несовпадения или нуль-терминатора)
        int match = 1;
        for(int j=0; j<20; j++) {
            if (dir[i].name[j] != name[j]) { match = 0; break; }
            if (name[j] == '\0') break;
        }

        if (match && dir[i].name[0] != 0) {
            char* data = (char*)kmalloc(512);
            read_sectors_ata_pio((uint64_t)data, dir[i].start_lba, 1);
            
            // Выводим содержимое в терминал
            data[63] = '\0'; // Ограничим длину, чтобы влезло в терминал
            term_print("--- File content ---");
            term_print(data);
            return;
        }
    }
    term_print("File not found.");
}