#include "../drivers/vga/vesa.h"
#include <stdint.h>

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t int_no;   
    uint64_t err_code; 
    uint64_t rip, cs, rflags, rsp, ss; 
} __attribute__((packed)) interrupt_frame_t;

void panic_handler(interrupt_frame_t* frame) {
    // 1. ЖЕСТКО ВЫКЛЮЧАЕМ ПРЕРЫВАНИЯ
    __asm__ volatile ("cli");

    // 2. Останавливаем другие ядра (если бы они были, но на будущее)
    // 3. Рисуем фон ОДИН РАЗ
    draw_rect_direct(0, 0, screen_width, screen_height, 0x880000);

    // 4. Простая проверка: если шрифт отвалился, нарисуем хотя бы белый квадрат в углу
    put_pixel_direct(10, 10, 0xFFFFFF); 
    put_pixel_direct(11, 10, 0xFFFFFF);

    // 5. Печатаем текст
    vesa_draw_string_direct("--- SYSTEM PANIC ---", 30, 30, 0xFFFFFF);
    
    // Выводим только самое важное, без лишних циклов
    vesa_draw_string_hex_direct("EXCEPTION: ", 30, 60, frame->int_no, 0xFFFF00);
    vesa_draw_string_hex_direct("RIP:       ", 30, 80, frame->rip, 0xFFFFFF);
    vesa_draw_string_hex_direct("ERR CODE:  ", 30, 100, frame->err_code, 0xCCCCCC);
    vesa_draw_string_hex_direct("RSP:       ", 30, 120, frame->rsp, 0xCCCCCC);

    // 6. МЕРТВАЯ ПЕТЛЯ
    while(1) {
        __asm__ volatile ("hlt");
    }
}