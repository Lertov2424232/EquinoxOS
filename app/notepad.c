#include <eid.h>
#include <equos.h>
#include <string.h>

#define WIN_W 500
#define WIN_H 350
uint32_t fb[WIN_W * WIN_H];
char text_buffer[1024] = "Equinox OS Notepad v2.0\nReady to type...";
bool running = true;

int main() {
  eid_init();
  eid_context_t gui;

  while (running) {
    eid_begin(&gui, fb, WIN_W, WIN_H);

    // Рисуем рамку окна
    eid_window_begin(&gui, "Notepad", &running);

    // Белое поле для текста (Sunken Panel)
    eid_draw_rect(fb, WIN_W, 10, 40, WIN_W - 20, WIN_H - 80,
                  EID_CLR_SURFACE_DP);
    eid_draw_text(fb, WIN_W, 20, 50, text_buffer, EID_CLR_TEXT);

    // Кнопка Clear (Очистить всё)
    if (eid_button(&gui, "Clear", 10, WIN_H - 35, 80, 25)) {
      text_buffer[0] = '\0';
    }

    // Кнопка Exit
    if (eid_button(&gui, "Exit", WIN_W - 90, WIN_H - 35, 80, 25)) {
      running = false;
    }

    // Обработка клавиш (очень упрощенно для теста)
    uint8_t key = (uint8_t)_syscall(SYS_GET_SCANCODE, 0, 0, 0, 0, 0);
    if (key == 0x1E)
      strcat(text_buffer, "a"); // Если нажали 'A'

    eid_end(&gui);
    sys_yield();
  }

  _syscall(SYS_EXIT, 0, 0, 0, 0, 0);
  return 0;
}