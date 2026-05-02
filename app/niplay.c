#include <eid.h>
#include <equos.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void print(const char* msg) { _syscall(SYS_PRINT, (uintptr_t)msg, 0, 0, 0, 0); }

#pragma pack(push, 1)
typedef struct {
    char riff_id[4];
    uint32_t riff_size;
    char wave_id[4];
} WavHeader;

typedef struct {
    char chunk_id[4];
    uint32_t chunk_size;
} ChunkHeader;

typedef struct {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} FmtChunk;
#pragma pack(pop)

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

    if (strncmp((char*)file_data, "RIFF", 4) != 0) {
        print("[NiPlay] ERR: Not a RIFF file\n");
        return 1;
    }

    FmtChunk fmt;
    uint8_t* audio_ptr = NULL;
    uint32_t audio_len = 0;
    int fmt_found = 0;

    uint32_t offset = 12;
    while (offset < file_size - 8) {
        ChunkHeader* ch = (ChunkHeader*)(file_data + offset);
        if (strncmp(ch->chunk_id, "fmt ", 4) == 0) {
            memcpy(&fmt, file_data + offset + 8, sizeof(FmtChunk));
            fmt_found = 1;
        } else if (strncmp(ch->chunk_id, "data", 4) == 0) {
            audio_len = ch->chunk_size;
            audio_ptr = file_data + offset + 8;
            break;
        }
        offset += 8 + ch->chunk_size;
    }

    if (!fmt_found || !audio_ptr) {
        print("[NiPlay] ERR: Invalid WAV file\n");
        return 1;
    }

    char dbg[128];
    sprintf(dbg, "[NiPlay] Playing: %s\n[NiPlay] Rate: %d Hz, Ch: %d, Bits: %d\n", 
            filename, fmt.sample_rate, fmt.num_channels, fmt.bits_per_sample);
    print(dbg);

    // Set hardware sample rate
    _syscall(SYS_AUDIO_SET_RATE, fmt.sample_rate, 0, 0, 0, 0);

    uint32_t chunk_size = 8192;
    
    uint32_t processed = 0;
    while (processed < audio_len) {
        uint32_t to_play = (audio_len - processed > chunk_size) ? chunk_size : (audio_len - processed);
        
        // Simple 16-bit stereo passthrough
        _syscall(SYS_AUDIO_PLAY, (uintptr_t)(audio_ptr + processed), (uint64_t)to_play, 0, 0, 0);
        
        processed += to_play;

        if ((uint8_t)_syscall(SYS_GET_SCANCODE, 0, 0, 0, 0, 0) == 0x01) break;
    }

    print("[NiPlay] Finished.\n");
    return 0;
}