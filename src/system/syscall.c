#include "syscall.h"
#include "../drivers/vga/vesa.h"
#include "../libc/stdio.h"

void syscall_handler(syscall_regs_t* regs) {
    // Номер вызова в RAX
    uint64_t num = regs->rax;

    switch(num) {
        case 1: // SYS_PRINT
            term_print((const char*)regs->rbx);
            break;
        case 4: // SYS_DRAW_PIXEL
            put_pixel(regs->rbx, regs->rcx, regs->rdx);
            break;
        case 5: // SYS_DRAW_BUFFER (x, y, w, h, buf)
            // Тут логика отрисовки приложения в окно
            break;
        case 8: // SYS_GET_MOUSE
            // Вернуть координаты мыши в регистры
            break;
        default:
            term_print("Unknown syscall!\n");
            break;
    }
}