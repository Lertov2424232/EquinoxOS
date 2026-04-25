// sdk/lib/posix.c
#include <stdint.h>
#include "../include/equos.h"

typedef struct {
    uint8_t* data;
    uint32_t size;
    uint32_t pos;
} FILE;

FILE* fopen(const char* filename, const char* mode) {
    uint32_t size = 0;
    // Твой системный вызов чтения файла целиком
    uint8_t* data = (uint8_t*)_syscall(2, (uint64_t)filename, (uint64_t)&size, 0, 0, 0);
    
    if (!data) return 0;

    FILE* f = (FILE*)malloc(sizeof(FILE));
    f->data = data;
    f->size = size;
    f->pos = 0;
    return f;
}

int fread(void* ptr, int size, int nmemb, FILE* stream) {
    if (!stream) return 0;
    int bytes_to_read = size * nmemb;
    if (stream->pos + bytes_to_read > stream->size) {
        bytes_to_read = stream->size - stream->pos;
    }
    
    // memcpy из твоего SDK
    for(int i=0; i<bytes_to_read; i++) {
        ((uint8_t*)ptr)[i] = stream->data[stream->pos + i];
    }
    
    stream->pos += bytes_to_read;
    return bytes_to_read / size;
}

int fseek(FILE* stream, long offset, int whence) {
    if (whence == 0) stream->pos = offset;      // SEEK_SET
    if (whence == 1) stream->pos += offset;     // SEEK_CUR
    if (whence == 2) stream->pos = stream->size + offset; // SEEK_END
    return 0;
}

long ftell(FILE* stream) { return stream->pos; }
int fclose(FILE* stream) { return 0; }