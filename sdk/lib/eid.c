#include <eid.h>
#include <string.h>

void eid_draw_rect(uint32_t* buf, int win_w, int x, int y, int w, int h, uint32_t color) {
    for (int i = y; i < y + h; i++) {
        for (int j = x; j < x + w; j++) {
            buf[i * win_w + j] = color;
        }
    }
}

// Кнопка в стиле классического Win95/Equinox
void eid_draw_button(uint32_t* buf, int win_w, int x, int y, int w, int h, const char* label, bool pressed) {
    uint32_t top_col = pressed ? 0x888888 : 0xFFFFFF;
    uint32_t bot_col = pressed ? 0xFFFFFF : 0x888888;
    
    eid_draw_rect(buf, win_w, x, y, w, h, EID_CLR_BG);
    
    // Рамка (эффект объема)
    eid_draw_rect(buf, win_w, x, y, w, 1, top_col); // Верх
    eid_draw_rect(buf, win_w, x, y, 1, h, top_col); // Лево
    eid_draw_rect(buf, win_w, x, y + h - 1, w, 1, bot_col); // Низ
    eid_draw_rect(buf, win_w, x + w - 1, y, 1, h, bot_col); // Право

    // Текст (центрирование)
    // eid_draw_text(...) - нужно будет прокинуть syscall для текста или вшить шрифт в SDK
}