#include <stdint.h>
#include "../drivers/vga/vesa.h"
#include "../libc/stdio.h"
#include "../gui/gui.h"

extern volatile uint32_t tick;
extern void sys_draw_app_buffer(int x, int y, int w, int h, uint32_t* buffer);

typedef struct {
    uint64_t r9, r8, rax, rbx, rcx, rdx, rsi, rdi, rbp;
    uint64_t rip, cs, rflags, rsp, ss; 
} syscall_regs_t;

void syscall_handler(syscall_regs_t* regs) {
    uint64_t num = regs->rax;

    switch (num) {
        case 1: // SYS_PRINT
            term_print((const char*)regs->rdi); 
            break;

        case 5: // SYS_DRAW_BUFFER
            sys_draw_app_buffer(regs->rdi, regs->rsi, regs->rdx, regs->rcx, (uint32_t*)regs->r8);
            break;

        case 6: // SYS_GET_TIME
            regs->rax = tick * 10; // Возвращаем время в RAX
            break;

        case 9: // SYS_GET_SCANCODE
            extern uint8_t last_scancode;
            regs->rax = last_scancode;
            last_scancode = 0;
            break;

        case 10: // SYS_EXIT
            term_print("[SYS] Application exited.\n");
            break;

        default:
            break;
    }
}