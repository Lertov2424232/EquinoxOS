#include "idt.h"

idt_gate_t idt[256];
idt_register_t idt_reg;

extern uint64_t isr_stub_table[];
extern void keyboard_handler();
extern void timer_handler();
extern void mouse_handler();
extern void irq0_handler_asm();
extern void syscall_interrupt_asm();

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
  // 1. Сначала СТРОГО подготавливаем структуру дескриптора
  idt_reg.limit = (uint16_t)(sizeof(idt_gate_t) * 256 - 1);
  idt_reg.base = (uint64_t)&idt;

  // 2. Заполняем таблицу
  uint16_t sel = 0x28; // Стандарт Limine
  for (int i = 0; i < 32; i++) {
    set_idt_gate(i, isr_stub_table[i], sel);
  }

  // Заглушки для IRQ
  set_idt_gate(32, (uint64_t)irq0_handler_asm, sel);
  set_idt_gate(32, (uint64_t)timer_handler, sel);
  set_idt_gate(33, (uint64_t)keyboard_handler, sel);
  set_idt_gate(44, (uint64_t)mouse_handler, sel);
  set_idt_gate(0x80, (uint64_t)syscall_interrupt_asm, 0x08);

  // 3. ЗАГРУЖАЕМ (Только эта строчка говорит процессору "смотри сюда")
  __asm__ __volatile__("lidt %0" : : "m"(idt_reg));
}