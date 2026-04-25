#include "doomgeneric.h"
#include "../../sdk/include/equos.h"

// 1. Инициализация: создаем окно или готовим буфер
void DG_Init() {
    // Тут можно ничего не делать, так как окно мы создадим при старте
}

// 2. Отрисовка кадра
void DG_DrawFrame() {
    // DG_ScreenBuffer - это массив пикселей (uint32_t), который дает Doom
    // Мы просто кидаем его в наше окно через системный вызов
    _syscall(SYS_DRAW_BUFFER, 0, 0, DOOMGENERIC_RESX, DOOMGENERIC_RESY, (uint64_t)DG_ScreenBuffer);
}

// 3. Сон (для ограничения FPS)
void DG_SleepMs(uint32_t ms) {
    sys_sleep(ms);
}

// 4. Время в миллисекундах
uint32_t DG_GetTicksMs() {
    return (uint32_t)_syscall(6, 0, 0, 0, 0, 0); // Твой SYS_GET_TIME
}

// 5. Ввод клавиатуры
int DG_GetKey(int* pressed, unsigned char* key) {
    uint8_t scancode = (uint8_t)_syscall(9, 0, 0, 0, 0, 0); // Твой SYS_GET_SCANCODE
    if (scancode == 0) return 0;

    *pressed = !(scancode & 0x80); // Нажата или отпущена
    uint8_t clean_scancode = scancode & 0x7F;

    // Мапим твои сканкоды на константы Doom
    if (clean_scancode == 0x11) { *key = DG_KEY_W; return 1; }
    if (clean_scancode == 0x1E) { *key = DG_KEY_A; return 1; }
    if (clean_scancode == 0x1F) { *key = DG_KEY_S; return 1; }
    if (clean_scancode == 0x20) { *key = DG_KEY_D; return 1; }
    if (clean_scancode == 0x1C) { *key = DG_KEY_ENTER; return 1; }
    if (clean_scancode == 0x01) { *key = DG_KEY_ESCAPE; return 1; }
    
    return 0;
}