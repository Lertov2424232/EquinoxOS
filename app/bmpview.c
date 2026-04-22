#include <equos.h>
#include <eid.h>
#include <stdint.h>

#pragma pack(push, 1)
typedef struct {
    uint16_t type;
    uint32_t size;
    uint16_t res1, res2;
    uint32_t offset;
} bmp_fh_t;

typedef struct {
    uint32_t size;
    int32_t  width;
    int32_t  height;
    uint16_t planes;
    uint16_t bit_count;
    uint32_t compression;
    uint32_t size_image;
    int32_t  x_ppm, y_ppm;
    uint32_t colors_used, colors_imp;
} bmp_ih_t;
#pragma pack(pop)

#define WIN_W 400
#define WIN_H 350
uint32_t app_buffer[WIN_W * WIN_H];

uint8_t get_key() { 
    return (uint8_t)_syscall(SYS_GET_SCANCODE, 0, 0, 0, 0, 0); 
}

int main(int argc, char** argv) {
    eid_init();

    char filename[16];
    // По умолчанию ставим "IMAGE.BMP" (по буквам, чтобы без багов strcpy)
    filename[0]='L'; filename[1]='O'; filename[2]='G'; filename[3]='O';
    filename[4]='.'; filename[5]='B'; filename[6]='M'; filename[7]='P'; filename[8]='\0';

    // Жесткий и надежный парсинг аргументов
    if (argc > 1 && argv[1] != 0) {
        char* arg = argv[1];
        int start = 0;
        
        // Пропускаем "--" если они есть
        if (arg[0] == '-' && arg[1] == '-') start = 2;
        
        int i = 0;
        // Копируем только видимые символы
        while (arg[start + i] != '\0' && arg[start + i] != '\r' && arg[start + i] != '\n' && arg[start + i] != ' ' && i < 15) {
            filename[i] = arg[start + i];
            i++;
        }
        filename[i] = '\0';
    }

    // --- ОТЛАДКА В ТЕРМИНАЛ (МЫ УВИДИМ, ЧТО ИМЕННО ОН ПАРСИТ) ---
    _syscall(SYS_PRINT, (uint64_t)"[APP] Trying to read: '", 0, 0, 0, 0);
    _syscall(SYS_PRINT, (uint64_t)filename, 0, 0, 0, 0);
    _syscall(SYS_PRINT, (uint64_t)"'\n", 0, 0, 0, 0);

    uint32_t file_size = 0;
    uint8_t* file_data = (uint8_t*)_syscall(SYS_READ_FILE, (uint64_t)filename, (uint64_t)&file_size, 0, 0, 0);

    // Чистим буфер
    for(int i=0; i < WIN_W * WIN_H; i++) app_buffer[i] = EID_CLR_BG;

    if (file_data) {
        _syscall(SYS_PRINT, (uint64_t)"[APP] File loaded SUCCESS!\n", 0, 0, 0, 0);
        eid_draw_window_frame(app_buffer, WIN_W, WIN_W, WIN_H, filename);
        
        bmp_fh_t* fh = (bmp_fh_t*)file_data;
        bmp_ih_t* ih = (bmp_ih_t*)(file_data + sizeof(bmp_fh_t));

        if (fh->type == 0x4D42) {
            uint8_t* pixels = file_data + fh->offset;
            int b_w = ih->width;
            int b_h = ih->height;
            int row_size = (b_w * 3 + 3) & ~3;

            for (int y = 0; y < b_h && y < (WIN_H - 40); y++) {
                for (int x = 0; x < b_w && x < (WIN_W - 20); x++) {
                    int py = b_h - 1 - y;
                    uint8_t* p = pixels + (py * row_size) + (x * 3);
                    uint32_t color = (p[2] << 16) | (p[1] << 8) | p[0];
                    app_buffer[(y + 35) * WIN_W + (x + 10)] = color;
                }
            }
        }
    } else {
        _syscall(SYS_PRINT, (uint64_t)"[APP] File read FAILED!\n", 0, 0, 0, 0);
        eid_draw_window_frame(app_buffer, WIN_W, WIN_W, WIN_H, "Error");
        eid_draw_text(app_buffer, WIN_W, 20, 50, "File not found:", EID_CLR_DANGER);
        eid_draw_text(app_buffer, WIN_W, 20, 70, filename, EID_CLR_TEXT);
    }

    _syscall(SYS_DRAW_BUFFER, 150, 150, WIN_W, WIN_H, (uint64_t)app_buffer);

    while(1) {
        uint8_t key = get_key();
        if (key == 0x01) break;
        sys_sleep(50); // Не проверяй кнопку 1000000 раз в секунду! 20 раз хватит.
    }

    _syscall(SYS_EXIT, 0, 0, 0, 0, 0);
    return 0;
}