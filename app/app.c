#include "../src/api.h" // Подключаем структуру API

// Точка входа теперь принимает структуру
void _start(EquinoxAPI* sys) {
    sys->print("Hello! I am a GUI App.");
    sys->print("Drawing a red box...");

    // Рисуем красный квадрат посередине экрана
    int x = sys->screen_width / 2 - 50;
    int y = sys->screen_height / 2 - 50;
    
    sys->draw_rect(x, y, 100, 100, 0xFF0000);
    sys->update_screen(); // Обязательно, чтобы увидеть результат!

    sys->print("Press ENTER to change color...");
    
    // Ждем ввода (простая заглушка)
    // Тут надо будет нажать любую букву + Enter, так как у нас буферизованный ввод пока
    char c = sys->get_key(); 

    sys->draw_rect(x, y, 100, 100, 0x00FF00); // Зеленый
    sys->update_screen();

    sys->print("Exiting in loop...");
    
    // Бесконечный цикл с анимацией
    int offset = 0;
    while(1) {
        // Очищаем старое место (черным)
        sys->draw_rect(x + offset, y, 10, 10, 0x000000);
        offset++;
        if (offset > 100) break; // Выход через время
        
        // Рисуем новое (синим)
        sys->draw_rect(x + offset, y, 10, 10, 0x0000FF);
        sys->update_screen();
        
        // Задержка
        for(int i=0; i<1000000; i++) __asm__("nop");
    }
}