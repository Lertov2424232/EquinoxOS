#include "../drivers/vga/vesa.h"
#include <stdint.h>

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t int_no;   
    uint64_t err_code; 
    uint64_t rip, cs, rflags, rsp, ss; 
} __attribute__((packed)) interrupt_frame_t;

// Расшифровка аппаратных исключений x86_64
static const char* exception_messages[32] = {
    "Division By Zero", "Debug", "Non-Maskable Interrupt", "Breakpoint",
    "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
    "Stack-Segment Fault", "General Protection Fault", "Page Fault", "Reserved",
    "x87 Floating-Point Exception", "Alignment Check", "Machine Check", "SIMD Floating-Point",
    "Virtualization Exception", "Control Protection Exception", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection", "VMM Communication", "Security Exception", "Reserved"
};

void panic_handler(interrupt_frame_t* frame) {
    // 1. ЖЕСТКО ВЫКЛЮЧАЕМ ПРЕРЫВАНИЯ
    __asm__ volatile ("cli");

    // Вытягиваем Control Registers (CR2 содержит адрес, вызвавший Page Fault)
    uint64_t cr2, cr3;
    __asm__ volatile ("mov %%cr2, %0" : "=r" (cr2));
    __asm__ volatile ("mov %%cr3, %0" : "=r" (cr3));

    // 3. Рисуем фон (Темно-синий, классический BSOD)
    draw_rect_direct(0, 0, screen_width, screen_height, 0x0000AA);

    // 4. Заголовок
    vesa_draw_string_direct("===============================================================", 30, 30, 0xFFFFFF);
    vesa_draw_string_direct(" A fatal hardware exception has occurred. EquinoxOS is halted. ", 30, 50, 0xFFFFFF);
    vesa_draw_string_direct("===============================================================", 30, 70, 0xFFFFFF);
    
    // 5. Какая именно ошибка произошла?
    if (frame->int_no < 32) {
        vesa_draw_string_direct(exception_messages[frame->int_no], 30, 110, 0x00FFFF);
    } else {
        vesa_draw_string_direct("Unknown Interrupt", 30, 110, 0x00FFFF);
    }
    
    vesa_draw_string_hex_direct("INT_NO: ", 300, 110, frame->int_no, 0xFFFFFF);
    vesa_draw_string_hex_direct("ERR_CD: ", 500, 110, frame->err_code, 0xFFFFFF);

    // 6. Управляющие регистры (КРИТИЧЕСКИ ВАЖНО ДЛЯ ДЕБАГА ПАМЯТИ)
    vesa_draw_string_direct("--- CONTROL REGISTERS ---", 30, 150, 0xAAAAFF);
    vesa_draw_string_hex_direct("CR2 (Fault Addr): ", 30,  170, cr2, 0xFF7777); // Красным!
    vesa_draw_string_hex_direct("CR3 (Page Dir)  : ", 350, 170, cr3, 0xFFFFFF);

    // 7. Регистры процессора (В две колонки)
    vesa_draw_string_direct("--- CPU REGISTERS ---", 30, 210, 0xAAAAFF);
    
    // Левая колонка
    vesa_draw_string_hex_direct("RAX: ", 30, 230, frame->rax, 0xFFFFFF);
    vesa_draw_string_hex_direct("RBX: ", 30, 250, frame->rbx, 0xFFFFFF);
    vesa_draw_string_hex_direct("RCX: ", 30, 270, frame->rcx, 0xFFFFFF);
    vesa_draw_string_hex_direct("RDX: ", 30, 290, frame->rdx, 0xFFFFFF);
    vesa_draw_string_hex_direct("RSI: ", 30, 310, frame->rsi, 0xFFFFFF);
    vesa_draw_string_hex_direct("RDI: ", 30, 330, frame->rdi, 0xFFFFFF);
    vesa_draw_string_hex_direct("RBP: ", 30, 350, frame->rbp, 0xFFFFFF);
    vesa_draw_string_hex_direct("RSP: ", 30, 370, frame->rsp, 0xFFFFFF);

    // Правая колонка
    vesa_draw_string_hex_direct("R8 : ", 250, 230, frame->r8,  0xFFFFFF);
    vesa_draw_string_hex_direct("R9 : ", 250, 250, frame->r9,  0xFFFFFF);
    vesa_draw_string_hex_direct("R10: ", 250, 270, frame->r10, 0xFFFFFF);
    vesa_draw_string_hex_direct("R11: ", 250, 290, frame->r11, 0xFFFFFF);
    vesa_draw_string_hex_direct("R12: ", 250, 310, frame->r12, 0xFFFFFF);
    vesa_draw_string_hex_direct("R13: ", 250, 330, frame->r13, 0xFFFFFF);
    vesa_draw_string_hex_direct("R14: ", 250, 350, frame->r14, 0xFFFFFF);
    vesa_draw_string_hex_direct("R15: ", 250, 370, frame->r15, 0xFFFFFF);

    // 8. Где именно мы упали (Точка выполнения)
    vesa_draw_string_direct("--- EXECUTION STATE ---", 500, 210, 0xAAAAFF);
    vesa_draw_string_hex_direct("RIP: ", 500, 230, frame->rip, 0xFFFF00); // Желтым, чтобы бросалось в глаза
    vesa_draw_string_hex_direct("CS : ", 500, 250, frame->cs, 0xFFFFFF);
    vesa_draw_string_hex_direct("FLG: ", 500, 270, frame->rflags, 0xFFFFFF);

    // 9. Дамп вершины стека (Первые 5 значений, куда указывает RSP)
    vesa_draw_string_direct("--- RAW STACK DUMP ---", 500, 310, 0xAAAAFF);
    uint64_t* stack = (uint64_t*)frame->rsp;
    
    // ПРОВЕРКА: Если RSP в юзер-спейсе (ниже 0xFFFF...) и мы в панике, 
    // лучше не лезть туда без проверки маппинга, иначе - рекурсивный краш.
    if (frame->rsp < 0x00007FFFFFFFFFFF) {
        vesa_draw_string_direct("User stack access unsafe", 500, 330, 0xFF7777);
    } else {
        for (int i = 0; i < 5; i++) {
            // Рискуем, только если это стек ядра
            vesa_draw_string_hex_direct("-> ", 500, 330 + (i * 20), stack[i], 0xCCCCCC);
        }
    }

    vesa_draw_string_direct("Please restart your computer.", 30, 420, 0x00FF00);

    // 10. МЕРТВАЯ ПЕТЛЯ
    while(1) {
        __asm__ volatile ("hlt");
    }
}