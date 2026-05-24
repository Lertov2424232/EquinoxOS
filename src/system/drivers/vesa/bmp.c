#include "bmp.h"
#include "vesa.h"
#include "../../../gui/gui.h"
#include "../../mem/memory.h"
#include "../../../syslibc/string.h"

void draw_bmp(const uint8_t* data, int start_x, int start_y) {
    bmp_file_header_t* file_header = (bmp_file_header_t*)data;
    if (file_header->type != 0x4D42) return;

    bmp_info_header_t* info_header = (bmp_info_header_t*)(data + sizeof(bmp_file_header_t));
    uint8_t* pixel_data = (uint8_t*)(data + file_header->offset);
    
    int width = info_header->width;
    int height = info_header->height;
    int row_size = (width * 3 + 3) & ~3; 

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int pixel_y = height - 1 - y; 
            uint8_t* p = pixel_data + (pixel_y * row_size) + (x * 3);
            uint32_t color = (p[2] << 16) | (p[1] << 8) | p[0];
            put_pixel(start_x + x, start_y + y, color);
        }
    }
}

// НОВАЯ ФУНКЦИЯ: Рисует BMP внутри конкретного окна
void bmp_draw_to_window(window_t* win, const uint8_t* data, int start_x, int start_y) {
    if (!win || !data) return;

    bmp_file_header_t* file_header = (bmp_file_header_t*)data;
    if (file_header->type != 0x4D42) return;

    bmp_info_header_t* info_header = (bmp_info_header_t*)(data + sizeof(bmp_file_header_t));
    uint8_t* pixel_data = (uint8_t*)(data + file_header->offset);
    
    int width = info_header->width;
    int height = info_header->height;
    int row_size = (width * 3 + 3) & ~3; 

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int pixel_y = height - 1 - y; 
            uint8_t* p = pixel_data + (pixel_y * row_size) + (x * 3);
            uint32_t color = (p[2] << 16) | (p[1] << 8) | p[0];

            // Используем локальную функцию окна!
            gui_window_put_pixel(win, start_x + x, start_y + y, color);
        }
    }
}

uint8_t* bmp_create_from_window(window_t* win, uint32_t* out_size) {
    if (!win) return NULL;

    int width = win->w;
    int height = win->h;
    
    // В BMP строки должны быть выровнены по 4 байта
    int row_size = (width * 3 + 3) & ~3; 
    int data_size = row_size * height;
    uint32_t file_size = sizeof(bmp_file_header_t) + sizeof(bmp_info_header_t) + data_size;
    
    uint8_t* buffer = (uint8_t*)kmalloc(file_size);
    if (!buffer) return NULL;
    memset(buffer, 0, file_size);

    // 1. Заполняем File Header
    bmp_file_header_t* fh = (bmp_file_header_t*)buffer;
    fh->type = 0x4D42; // "BM"
    fh->size = file_size;
    fh->offset = sizeof(bmp_file_header_t) + sizeof(bmp_info_header_t);

    // 2. Заполняем Info Header
    bmp_info_header_t* ih = (bmp_info_header_t*)(buffer + sizeof(bmp_file_header_t));
    ih->size = sizeof(bmp_info_header_t);
    ih->width = width;
    ih->height = height; // Положительное значение = снизу вверх
    ih->planes = 1;
    ih->bit_count = 24;
    ih->compression = 0;
    ih->size_image = data_size;

    // 3. Копируем пиксели (Конвертируем 32-бит ARGB в 24-бит BGR)
    // Внимание: BMP хранится СНИЗУ ВВЕРХ
    uint8_t* pixel_ptr = buffer + fh->offset;
    for (int y = 0; y < height; y++) {
        uint8_t* row = pixel_ptr + (height - 1 - y) * row_size;
        for (int x = 0; x < width; x++) {
            uint32_t argb = win->buffer[y * width + x];
            
            row[x * 3 + 0] = (argb >> 0)  & 0xFF; // B
            row[x * 3 + 1] = (argb >> 8)  & 0xFF; // G
            row[x * 3 + 2] = (argb >> 16) & 0xFF; // R
        }
    }

    *out_size = file_size;
    return buffer;
}