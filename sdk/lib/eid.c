#include <eid.h>
#include <string.h>
#include <equos.h>

static psf1_t* internal_font = NULL;

void eid_init() {
    internal_font = (psf1_t*)get_system_font();
}

void eid_put_pixel(uint32_t* buf, int win_w, int x, int y, uint32_t color) {
    buf[y * win_w + x] = color;
}

void eid_draw_rect(uint32_t* buf, int win_w, int x, int y, int w, int h, uint32_t color) {
    for (int i = y; i < y + h; i++) {
        for (int j = x; j < x + w; j++) {
            buf[i * win_w + j] = color;
        }
    }
}

// Внутренняя функция для рисования прямоугольников со "спиленными" углами (псевдо-скругление)
static void _eid_draw_rounded_rect(uint32_t* buf, int win_w, int x, int y, int w, int h, uint32_t color) {
    eid_draw_rect(buf, win_w, x + 1, y, w - 2, h, color);
    eid_draw_rect(buf, win_w, x, y + 1, 1, h - 2, color);
    eid_draw_rect(buf, win_w, x + w - 1, y + 1, 1, h - 2, color);
}

// Внутренняя функция для рамки со скругленными углами
static void _eid_draw_rounded_border(uint32_t* buf, int win_w, int x, int y, int w, int h, uint32_t color) {
    eid_draw_rect(buf, win_w, x + 1, y, w - 2, 1, color);           // Top
    eid_draw_rect(buf, win_w, x + 1, y + h - 1, w - 2, 1, color);   // Bottom
    eid_draw_rect(buf, win_w, x, y + 1, 1, h - 2, color);           // Left
    eid_draw_rect(buf, win_w, x + w - 1, y + 1, 1, h - 2, color);   // Right
}

void eid_draw_text(uint32_t* buf, int win_w, int x, int y, const char* text, uint32_t color) {
    if (!internal_font) return;

    while (*text) {
        uint8_t* glyph = (uint8_t*)internal_font + sizeof(psf1_t) + ((uint8_t)*text * internal_font->charsize);
        
        for (int cy = 0; cy < internal_font->charsize; cy++) {
            for (int cx = 0; cx < 8; cx++) {
                if ((*glyph >> (7 - cx)) & 1) {
                    buf[(y + cy) * win_w + (x + cx)] = color;
                }
            }
            glyph++;
        }
        x += 8;
        text++;
    }
}

// Современная панель (Sunken - темный фон для инпутов, Выпуклая - обычная карточка)
void eid_draw_panel(uint32_t* buf, int win_w, int x, int y, int w, int h, bool sunken) {
    uint32_t bg_color = sunken ? EID_CLR_SURFACE_DP : EID_CLR_SURFACE;
    
    // Рисуем скругленное тело панели
    _eid_draw_rounded_rect(buf, win_w, x, y, w, h, bg_color);
    
    // Тонкая рамка-обводка
    _eid_draw_rounded_border(buf, win_w, x, y, w, h, EID_CLR_BORDER);
}

// Современная плоская кнопка с состояниями
void eid_draw_button(uint32_t* buf, int win_w, int x, int y, int w, int h, const char* label, int state) {
    uint32_t bg_color     = EID_CLR_SURFACE;
    uint32_t border_color = EID_CLR_BORDER;
    uint32_t text_color   = EID_CLR_TEXT;

    if (state == EID_STATE_HOVER) {
        bg_color = EID_CLR_BORDER; // Немного светлее фона
    } else if (state == EID_STATE_PRESSED) {
        bg_color = EID_CLR_ACCENT;
        border_color = EID_CLR_ACCENT;
        text_color = EID_CLR_TEXT_DARK; // Темный текст на светлом акценте
    } else if (state == EID_STATE_DISABLED) {
        bg_color = EID_CLR_SURFACE_DP;
        border_color = EID_CLR_SURFACE_DP;
        text_color = EID_CLR_BORDER;
    }

    // Отрисовка тела кнопки
    _eid_draw_rounded_rect(buf, win_w, x, y, w, h, bg_color);
    _eid_draw_rounded_border(buf, win_w, x, y, w, h, border_color);

    // Центрирование текста
    int text_len = strlen(label);
    int text_x = x + (w / 2) - (text_len * 4);
    int text_y = y + (h / 2) - (internal_font ? (internal_font->charsize / 2) : 8);

    // Эффект микро-нажатия
    if (state == EID_STATE_PRESSED) {
        text_x++;
        text_y++;
    }

    eid_draw_text(buf, win_w, text_x, text_y, label, text_color);
}

// Современное окно (без толстых рамок, с монолитным дизайном)
void eid_draw_window_frame(uint32_t* buf, int win_w, int w, int h, const char* title) {
    // Тень / Бордюр окна
    _eid_draw_rounded_border(buf, win_w, 0, 0, w, h, EID_CLR_ACCENT);
    
    // Основной фон окна
    _eid_draw_rounded_rect(buf, win_w, 1, 1, w - 2, h - 2, EID_CLR_BG);
    
    // Заголовок (Title Bar) - монолитный прямоугольник сверху
    eid_draw_rect(buf, win_w, 1, 1, w - 2, 28, EID_CLR_SURFACE);
    eid_draw_rect(buf, win_w, 1, 28, w - 2, 1, EID_CLR_BORDER); // Разделитель
    
    // Текст заголовка
    eid_draw_text(buf, win_w, 12, 6, title, EID_CLR_TEXT);
    
    // Кнопка закрытия (Современный минимализм: красная точка/квадратик)
    int btn_size = 14;
    int btn_x = w - btn_size - 8;
    int btn_y = 7;
    _eid_draw_rounded_rect(buf, win_w, btn_x, btn_y, btn_size, btn_size, EID_CLR_DANGER);
    
    // Иконка крестика
    eid_draw_text(buf, win_w, btn_x + 3, btn_y - 1, "x", EID_CLR_TEXT_DARK);
}

// Современный чекбокс (акцентный квадрат с галочкой)
void eid_draw_checkbox(uint32_t* buf, int win_w, int x, int y, const char* label, bool checked) {
    int box_size = 16;
    
    // Фон и обводка
    _eid_draw_rounded_rect(buf, win_w, x, y, box_size, box_size, EID_CLR_SURFACE_DP);
    _eid_draw_rounded_border(buf, win_w, x, y, box_size, box_size, checked ? EID_CLR_ACCENT : EID_CLR_BORDER);

    if (checked) {
        // Внутренний акцентный квадрат (заполненный чекбокс)
        _eid_draw_rounded_rect(buf, win_w, x + 3, y + 3, box_size - 6, box_size - 6, EID_CLR_ACCENT);
    }

    // Текст чекбокса
    eid_draw_text(buf, win_w, x + box_size + 8, y, label, EID_CLR_TEXT);
}

// Современный Progress Bar (Тонкий, стильный)
void eid_draw_progressbar(uint32_t* buf, int win_w, int x, int y, int w, int h, int progress) {
    if (progress < 0) progress = 0;
    if (progress > 100) progress = 100;

    // Трек прогресс-бара
    _eid_draw_rounded_rect(buf, win_w, x, y, w, h, EID_CLR_SURFACE_DP);
    _eid_draw_rounded_border(buf, win_w, x, y, w, h, EID_CLR_BORDER);

    // Заливка
    if (progress > 0) {
        int fill_w = ((w - 2) * progress) / 100;
        if (fill_w > 0) {
            _eid_draw_rounded_rect(buf, win_w, x + 1, y + 1, fill_w, h - 2, EID_CLR_ACCENT);
        }
    }
}