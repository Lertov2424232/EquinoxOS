#include <stdint.h>
#include "../drivers/vga/vesa.h"
#include "../libc/stdio.h"
#include "../libc/string.h"
#include "../gui/gui.h"
#include "vmm.h"
#include "pmm.h"
#include "../fs/fat32.h"
#include "memory.h"

extern volatile uint32_t tick;
extern void sys_draw_app_buffer(int x, int y, int w, int h, uint32_t* buffer);
extern uint8_t keyboard_pop();

typedef struct {
    uint64_t rax; // syscall_number
    uint64_t r9;
    uint64_t r8;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t rip, cs, rflags, rsp, ss;
} syscall_regs_t;

uint64_t copy_to_user(void* kernel_buf, uint64_t size) {
    if (!kernel_buf || size == 0) return 0;

    uint64_t pages = (size + 4095) / 4096;
    // 1. Находим свободные физические страницы
    void* phys = pmm_alloc_continuous(pages);
    
    // 2. Придумываем виртуальный адрес в пространстве юзера (например, в районе 0x80000000)
    // В идеале тут должен быть полноценный user_heap_alloc
    static uint64_t user_dynamic_ptr = 0x80000000; 
    uint64_t target_virt = user_dynamic_ptr;
    user_dynamic_ptr += (pages * 4096);

    // 3. Мапим их в текущий CR3 (который принадлежит процессу)
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    
    for (uint64_t i = 0; i < pages; i++) {
        vmm_map((page_table_t*)VIRT(cr3), 
                target_virt + (i * 4096), 
                (uint64_t)phys + (i * 4096), 
                PTE_USER | PTE_WRITABLE);
    }

    // 4. Копируем данные из ядра в эти новые страницы юзера
    memcpy((void*)target_virt, kernel_buf, size);

    return target_virt;
}

void syscall_handler(syscall_regs_t* regs) {
    uint64_t num = regs->rax;

    switch (num) {
        case 1: // SYS_PRINT
            term_print((const char*)regs->rdi); 
            break;
        case 2: { // SYS_READ_FILE
            uint32_t size = 0;
            uint8_t* kdata = fat32_read_file((const char*)regs->rdi, &size);
            if (kdata) {
                // АНТИ-КРАШ: Если драйвер FAT32 вернул физический адрес (ниже HHDM),
                // мы ОБЯЗАНЫ перевести его в виртуальный через HHDM!
                // Потому что в таблицах пользователя (где мы сейчас находимся)
                // нет маппинга физической памяти 1:1.
                uint64_t kdata_addr = (uint64_t)kdata;
                if (kdata_addr < hhdm_offset) {
                    kdata_addr = VIRT(kdata_addr);
                }

                // Копируем данные файла в память, доступную Ring 3
                regs->rax = copy_to_user((void*)kdata_addr, size);
                
                // Записываем размер (RSI — адрес в памяти юзера)
                if (regs->rsi) *(uint32_t*)regs->rsi = size;
                
                // ВНИМАНИЕ: Если fat32_read_file использует pmm_alloc, 
                // то тут нужно pmm_free. Если kmalloc - то kfree.
                kfree(kdata); 
            } else {
                regs->rax = 0;
            }
            break;
        }
        case 3: // SYS_WRITE_FILE (name: rdi, buf: rsi, size: rdx)
            fat32_save_file((const char*)regs->rdi, (const char*)regs->rsi, (uint32_t)regs->rdx);
            break;

        case 5: // SYS_DRAW_BUFFER
            sys_draw_app_buffer(regs->rdi, regs->rsi, regs->rdx, regs->rcx, (uint32_t*)regs->r8);
            break;

        case 6: // SYS_GET_TIME
            regs->rax = tick * 10; // Возвращаем время в RAX
            break;

        case 9: // SYS_GET_SCANCODE
            regs->rax = keyboard_pop(); // Вызываем функцию, получаем сканкод
            break;

        case 10: // SYS_EXIT
            term_print("[SYS] Killing process...\n");
            extern bool is_app_running;
            is_app_running = false;
            break;
        case 12: { // SYS_GET_FONT
            extern void* vesa_get_font();
            void* kfont = vesa_get_font();
            
            uint64_t font_addr = (uint64_t)kfont;
            if (font_addr < hhdm_offset) {
                font_addr = VIRT(font_addr);
            }
            
            regs->rax = copy_to_user((void*)font_addr, 4096);
            break;
        }

        default:
            break;
    }
}