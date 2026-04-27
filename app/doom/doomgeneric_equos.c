#include "doomgeneric.h"
#include "doomkeys.h"
#include "../../sdk/include/equos.h"
#include <stdio.h>

extern void doomgeneric_Create(int argc, char **argv);
extern void doomgeneric_Tick(); // Проверь, есть ли она, если нет - Create сам должен крутить цикл

void DG_Init() {
    // Чистим буфер клавиатуры при старте, чтобы не поймать случайный ESC
    for(int i = 0; i < 16; i++) _syscall(9, 0, 0, 0, 0, 0);
}

void DG_DrawFrame() {
    // ВАЖНО: Рисуем по координатам (0,0) в окне приложения
    _syscall(SYS_DRAW_BUFFER, 0, 0, DOOMGENERIC_RESX, DOOMGENERIC_RESY, (uintptr_t)DG_ScreenBuffer);
}

void DG_SleepMs(uint32_t ms) {
    sys_sleep(ms);
}

uint32_t DG_GetTicksMs() {
    return (uint32_t)_syscall(6, 0, 0, 0, 0, 0);
}

int DG_GetKey(int* pressed, unsigned char* key) {
    uint8_t scancode = (uint8_t)_syscall(SYS_GET_SCANCODE, 0, 0, 0, 0, 0);
    if (scancode == 0) return 0;

    static int is_extended = 0;
    if (scancode == 0xE0) {
        is_extended = 1;
        return 0;
    }

    *pressed = !(scancode & 0x80);
    uint8_t clean = scancode & 0x7F;

    if (is_extended) {
        is_extended = 0;
        switch(clean) {
            case 0x48: *key = KEY_UPARROW; break;
            case 0x50: *key = KEY_DOWNARROW; break;
            case 0x4B: *key = KEY_LEFTARROW; break;
            case 0x4D: *key = KEY_RIGHTARROW; break;
            case 0x1D: *key = KEY_FIRE; break; // Правый Ctrl -> ОГОНЬ
            default: return 0;
        }
        return 1;
    }

    switch(clean) {
        case 0x1D: *key = KEY_FIRE; break;   // Левый Ctrl -> ОГОНЬ
        case 0x39: *key = KEY_USE; break;    // Пробел -> ОТКРЫТЬ (Use)
        case 0x01: *key = KEY_ESCAPE; break;
        case 0x1C: *key = KEY_ENTER; break;
        case 0x0E: *key = KEY_BACKSPACE; break;
        
        default: {
            // Исправленный маппинг (скан-коды набора 1)
            static const char map[] = {
                0, 0, '1','2','3','4','5','6','7','8','9','0','-','=', 0, // 0-14
                9, 'q','w','e','r','t','y','u','i','o','p','[',']', 13, 0, // 15-29 (15 - Tab, 28 - Enter)
                0, 'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\', // 30-44 (30 - LCtrl, но он выше)
                'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' ' // 45-58
            };
            if (clean < sizeof(map) && map[clean] != 0) {
                *key = map[clean];
                return 1;
            }
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv) {
    printf("Equos: Starting Doom engine...\n");
    doomgeneric_Create(argc, argv);
    
    // Если мы дошли сюда, значит D_DoomMain вернулся.
    // В нормальной ситуации мы должны войти в бесконечный цикл тиков,
    // если doomgeneric_Create не сделал этого сам.
    while(1) {
        doomgeneric_Tick();
    }

    return 0;
}