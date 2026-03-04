#include "idt.h"

idt_gate_t idt[256];
idt_register_t idt_reg;

// Импортируем только один обработчик для теста
extern void isr0(); 
extern void keyboard_handler();
extern void timer_handler();
extern void mouse_handler();

void set_idt_gate(int n, uint64_t handler, uint16_t sel) {
    idt[n].low_offset = (uint16_t)(handler & 0xFFFF);
    idt[n].sel = sel;
    idt[n].ist = 0;
    idt[n].flags = 0x8E; 
    idt[n].mid_offset = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[n].high_offset = (uint32_t)(handler >> 32); 
    idt[n].reserved = 0;
}

void init_idt() {
    idt_reg.limit = sizeof(idt) - 1;
    idt_reg.base = (uint64_t)&idt;

    // Сначала всё забиваем одной заглушкой (например, isr0)
    // 0x28 - это стандартный селектор кода в Limine.
    for (int i = 0; i < 256; i++) {
        set_idt_gate(i, (uint64_t)isr0, 0x28);
    }

    // Ставим реальные прерывания
    set_idt_gate(32, (uint64_t)timer_handler, 0x28);
    set_idt_gate(33, (uint64_t)keyboard_handler, 0x28);
    set_idt_gate(44, (uint64_t)mouse_handler, 0x28);

    __asm__ __volatile__("lidt %0" : : "m" (idt_reg));
}