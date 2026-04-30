#include <eid.h>
#include <equos.h>
#include <stdint.h>

// Используем системный принт напрямую
void print(const char* msg) {
    _syscall(SYS_PRINT, (uint64_t)msg, 0, 0, 0, 0);
}

#pragma pack(push, 1)
typedef struct {
    char riff[4];
    uint32_t size;
    char wave[4];
    char fmt[4];
    uint32_t fmt_len;
    uint16_t format;
    uint16_t channels;
    uint32_t samplerate;
    uint32_t byterate;
    uint16_t align;
    uint16_t bits;
    char data_tag[4];
    uint32_t data_len;
} wav_header_t;
#pragma pack(pop)

#define WIN_W 400
#define WIN_H 200
uint32_t app_buffer[WIN_W * WIN_H];

int main(int argc, char** argv) {
    eid_init();
    
    char* filename = "MUSIC.WAV";
    if (argc > 1) filename = argv[1];

    print("[NiPlay] Attempting to play: ");
    print(filename);
    print("\n");

    // 1. Читаем файл целиком (как в bmpview)
    uint32_t file_size = 0;
    uint8_t* file_data = (uint8_t*)_syscall(SYS_READ_FILE, (uint64_t)filename, (uint64_t)&file_size, 0, 0, 0);

    if (!file_data) {
        print("[NiPlay] ERROR: File not found or empty!\n");
        _syscall(SYS_EXIT, 1, 0, 0, 0, 0);
    }

    // 2. Парсим заголовок
    wav_header_t* header = (wav_header_t*)file_data;
    
    // Простейшая проверка, что это WAV
    if (header->riff[0] != 'R' || header->wave[0] != 'W') {
        print("[NiPlay] ERROR: Not a valid RIFF/WAVE file!\n");
        _syscall(SYS_EXIT, 1, 0, 0, 0, 0);
    }

    // Ищем начало данных (пропускаем возможные JUNK чанки)
    uint8_t* audio_ptr = file_data + 44; 
    // В идеале нужно искать "data" по всему заголовку, но для простых WAV 44 - стандарт.
    uint32_t remaining = header->data_len;

    print("[NiPlay] Started playing...\n");

    // Очистка и отрисовка окна
    for (int i = 0; i < WIN_W * WIN_H; i++) app_buffer[i] = EID_CLR_BG;
    eid_draw_window_frame(app_buffer, WIN_W, WIN_W, WIN_H, "NiPlay");
    eid_draw_text(app_buffer, WIN_W, 30, 60, "Now Playing:", EID_CLR_TEXT);
    eid_draw_text(app_buffer, WIN_W, 30, 80, filename, EID_CLR_ACCENT);
    eid_draw_text(app_buffer, WIN_W, 30, 150, "Press ESC to Exit", 0x888888);
    _syscall(SYS_DRAW_BUFFER, 200, 200, WIN_W, WIN_H, (uint64_t)app_buffer);

    // 3. Цикл воспроизведения
    while (remaining > 0) {
        uint32_t chunk = (remaining > 8192) ? 8192 : remaining;
        
        // Отправляем чанк в AC97 через твой syscall 20
        _syscall(SYS_AUDIO_PLAY, (uint64_t)audio_ptr, (uint64_t)chunk, 0, 0, 0);
        
        audio_ptr += chunk;
        remaining -= chunk;

        // Проверка ESC
        if ((uint8_t)_syscall(SYS_GET_SCANCODE, 0, 0, 0, 0, 0) == 0x01) break;
        
        // Небольшой yield, чтобы GUI не замерзал намертво
        _syscall(SYS_YIELD, 0, 0, 0, 0, 0);
    }

    print("[NiPlay] Done.\n");
    _syscall(SYS_EXIT, 0, 0, 0, 0, 0);
    return 0;
}