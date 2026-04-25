#include "doomgeneric.h"
#include "doomkeys.h" // Подключаем оригинальные клавиши Doom
#include "../../sdk/include/equos.h"

extern void doomgeneric_Create(int argc, char **argv);

void DG_Init() {
    // Вызывается 1 раз при старте
}

void DG_DrawFrame() {
    // Кидаем кадр в наше окно
    _syscall(SYS_DRAW_BUFFER, 0, 0, DOOMGENERIC_RESX, DOOMGENERIC_RESY, (uint64_t)DG_ScreenBuffer);
}

void DG_SleepMs(uint32_t ms) {
    sys_sleep(ms);
}

uint32_t DG_GetTicksMs() {
    return (uint32_t)_syscall(6, 0, 0, 0, 0, 0); // Твой SYS_GET_TIME
}

int DG_GetKey(int* pressed, unsigned char* key) {
    uint8_t scancode = (uint8_t)_syscall(9, 0, 0, 0, 0, 0); // SYS_GET_SCANCODE
    if (scancode == 0) return 0;

    *pressed = !(scancode & 0x80); // Если 8-й бит равен 0 — кнопка нажата
    uint8_t clean_scancode = scancode & 0x7F;

    // --- БУКВЫ ---
    if (clean_scancode == 0x11) { *key = 'w'; return 1; }
    if (clean_scancode == 0x1E) { *key = 'a'; return 1; }
    if (clean_scancode == 0x1F) { *key = 's'; return 1; }
    if (clean_scancode == 0x20) { *key = 'd'; return 1; }
    if (clean_scancode == 0x12) { *key = 'e'; return 1; } // E - открыть дверь

    // --- СПЕЦКЛАВИШИ ---
    if (clean_scancode == 0x1C) { *key = KEY_ENTER; return 1; }
    if (clean_scancode == 0x01) { *key = KEY_ESCAPE; return 1; }
    if (clean_scancode == 0x39) { *key = ' '; return 1; } // Пробел
    if (clean_scancode == 0x1D) { *key = KEY_RCTRL; return 1; } // Правый CTRL

    // --- СТРЕЛОЧКИ ---
    if (clean_scancode == 0x48) { *key = KEY_UPARROW; return 1; }
    if (clean_scancode == 0x50) { *key = KEY_DOWNARROW; return 1; }
    if (clean_scancode == 0x4B) { *key = KEY_LEFTARROW; return 1; }
    if (clean_scancode == 0x4D) { *key = KEY_RIGHTARROW; return 1; }
    
    return 0;
}

int main(int argc, char **argv) {
    // Вызываем правильную функцию инициализации порта
    doomgeneric_Create(argc, argv);
    
    // После этого управление перейдет в движок Дума
    return 0;
}