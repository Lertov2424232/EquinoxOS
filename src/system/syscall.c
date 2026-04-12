#include "../drivers/vga/vesa.h"
#include "../gui/gui.h"
#include "../libc/stdio.h"
#include <stdint.h>


// Добавляем внешние переменные
extern volatile uint32_t tick;
extern void sys_draw_app_buffer(int x, int y, int w, int h, uint32_t *buffer);

// Новая структура: порядок должен быть строго обратным push в ASM
typedef struct {
  uint64_t r9;
  uint64_t r8;
  uint64_t rax;
  uint64_t rbx;
  uint64_t rcx;
  uint64_t rdx;
  uint64_t rsi;
  uint64_t rdi;
  uint64_t rbp;
  // Данные процессора
  uint64_t rip, cs, rflags, rsp, ss;
} syscall_regs_t;

void syscall_handler(syscall_regs_t *regs) {
  uint64_t num = regs->rax;

  switch (num) {
  case 1:                                // SYS_PRINT
    term_print((const char *)regs->rdi); // Первый аргумент syscall - RDI
    break;

  case 5: // SYS_DRAW_BUFFER (x, y, w, h, buf)
    // rdi=x, rsi=y, rdx=w, rcx=h, r8=buf
    sys_draw_app_buffer(regs->rdi, regs->rsi, regs->rdx, regs->rcx,
                        (uint32_t *)regs->r8);
    break;

  case 6: // SYS_GET_TIME
    regs->rax = tick * 10;
    break;

  case 9: // SYS_GET_SCANCODE
    extern uint8_t last_scancode;
    regs->rax = last_scancode;
    last_scancode = 0;
    break;

  case 10: // SYS_EXIT
    // В будущем тут будет уничтожение процесса
    break;
  }
}