#include <eid.h>
#include <equos.h>
#include <stdint.h>
#include <stdio.h>

void print(const char* msg) { _syscall(SYS_PRINT, (uintptr_t)msg, 0, 0, 0, 0); }

int main(int argc, char** argv) {
    eid_init();
    char* filename = "MUSIC.WAV";
    if (argc > 1) filename = argv[1];

    uint32_t file_size = 0;
    uint8_t* file_data = (uint8_t*)_syscall(SYS_READ_FILE, (uintptr_t)filename, (uintptr_t)&file_size, 0, 0, 0);

    if (!file_data) {
        print("[NiPlay] ERR: File not found\n");
        return 1;
    }

    // Поиск чанка "data" вручную
    uint8_t* audio_ptr = NULL;
    uint32_t audio_len = 0;
    uint32_t sample_rate = 0;

    // Пройдемся по файлу и найдем заголовки
    for (uint32_t i = 0; i < 200 && i < file_size - 8; i++) {
        // Ищем частоту дискретизации (она обычно в чанке 'fmt ')
        if (file_data[i] == 'f' && file_data[i+1] == 'm' && file_data[i+2] == 't') {
            sample_rate = *(uint32_t*)&file_data[i + 8];
        }
        // Ищем начало данных 'data'
        if (file_data[i] == 'd' && file_data[i+1] == 'a' && file_data[i+2] == 't' && file_data[i+3] == 'a') {
            audio_len = *(uint32_t*)&file_data[i + 4];
            audio_ptr = &file_data[i + 8];
            break; 
        }
    }

    if (!audio_ptr) {
        print("[NiPlay] ERR: Could not find 'data' chunk. Playing as raw...\n");
        audio_ptr = file_data + 44;
        audio_len = file_size - 44;
    }

    char dbg[128];
    sprintf(dbg, "[NiPlay] Playing: %s\n[NiPlay] Rate: %d Hz, Len: %d KB\n", 
            filename, sample_rate, audio_len / 1024);
    print(dbg);

    if (sample_rate != 44100) {
        print("[NiPlay] WARNING: Hardware is 44100Hz, file is different. Pitch will be wrong.\n");
    }

    // Отрисовка UI
    uint32_t* buf = (uint32_t*)malloc(400 * 200 * 4); // Попробуем malloc для буфера окна
    if (!buf) {
         print("[NiPlay] Malloc failed, using emergency buffer\n");
         // Если malloc упал, значит 16МБ файл сожрал всё. 
         // Но мы просто продолжим без GUI или с системным вызовом.
    }

    uint32_t remaining = audio_len;
    uint32_t chunk_size = 8192;
    int counter = 0;

    while (remaining > 0) {
        uint32_t to_play = (remaining > chunk_size) ? chunk_size : remaining;
        
        // Системный вызов звука
        _syscall(SYS_AUDIO_PLAY, (uintptr_t)audio_ptr, (uint64_t)to_play, 0, 0, 0);
        
        audio_ptr += to_play;
        remaining -= to_play;

        if (++counter % 100 == 0) {
            sprintf(dbg, "[NiPlay] Progress: %d KB left\n", remaining / 1024);
            print(dbg);
        }

        if ((uint8_t)_syscall(SYS_GET_SCANCODE, 0, 0, 0, 0, 0) == 0x01) break;
    }

    print("[NiPlay] Finished.\n");
    return 0;
}