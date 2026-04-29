#include "../drivers/vga/vesa.h"
#include "../fs/fat32.h"
#include "../gui/gui.h"
#include "../libc/stdio.h"
#include "../libc/string.h"
#include "task.h"
#include "memory.h"
#include "pmm.h"
#include "vmm.h"
#include <stdint.h>
#include "../drivers/audio/ac97.h"

extern volatile uint32_t tick;
extern void sys_draw_app_buffer(int x, int y, int w, int h, uint32_t *buffer);
extern uint8_t keyboard_pop();
extern void term_print(const char *str);

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

uint64_t copy_to_user(void *kernel_buf, uint64_t size) {
    if (!kernel_buf || size == 0) return 0;

    uint64_t pages = (size + 4095) / 4096;
    static uint64_t user_dynamic_ptr = 0x60000000;
    uint64_t target_virt = user_dynamic_ptr;
    user_dynamic_ptr += (pages * 4096);

    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    page_table_t* pml4 = (page_table_t *)VIRT(cr3);

    for (uint64_t i = 0; i < pages; i++) {
        // Берем ЛЮБУЮ свободную страницу, не обязательно подряд!
        void *phys = pmm_alloc(); 
        if (!phys) return 0;

        vmm_map(pml4, target_virt + (i * 4096), (uint64_t)phys, 
                PTE_PRESENT | PTE_USER | PTE_WRITABLE);
        
        // Копируем по кусочкам
        uint64_t to_copy = (size > 4096) ? 4096 : size;
        memcpy((void *)(target_virt + (i * 4096)), (uint8_t*)kernel_buf + (i * 4096), to_copy);
        size -= to_copy;
    }

    return target_virt;
}

void syscall_handler(syscall_regs_t *regs) {
  uint64_t num = regs->rax;

  switch (num) {
  case 1: // SYS_PRINT
    term_print((const char *)regs->rdi);
    break;
  case 2: { // SYS_READ_FILE
    uint32_t size = 0;
    // 1. Читаем файл в буфер ядра
    uint8_t *kdata = fat32_read_file((const char *)regs->rdi, &size);
    
    if (!kdata) {
        regs->rax = 0; // Возвращаем NULL если файл не найден
        break;
    }

    // 2. Если пользователь передал указатель в RSI, записываем туда размер файла
    if (regs->rsi) {
        uint32_t *user_size_ptr = (uint32_t*)regs->rsi;
        *user_size_ptr = size;
    }

    // 3. Копируем данные из ядра в пространство процесса
    // copy_to_user сама выделит страницы и замапит их
    uint64_t user_ptr = copy_to_user(kdata, size);

    // 4. Освобождаем временный буфер в ядре (данные уже скопированы юзеру)
    kfree(kdata);

    // 5. Возвращаем указатель на данные процесса в RAX
    regs->rax = user_ptr;
    break;
  }
  case 3: // SYS_WRITE_FILE (name: rdi, buf: rsi, size: rdx)
    // fat32_save_file((const char *)regs->rdi, (const char *)regs->rsi,
    //                 (uint32_t)regs->rdx);
    break;

  case 5: // SYS_DRAW_BUFFER
    sys_draw_app_buffer(regs->rdi, regs->rsi, regs->rdx, regs->rcx,
                        (uint32_t *)regs->r8);
    break;

  case 6:                  // SYS_GET_TIME
    regs->rax = tick * 10; // Возвращаем время в RAX
    break;

  case 9: // SYS_GET_SCANCODE
{
    static window_t* last_focus = NULL;
    
    if (focused_window == app_win) {
        // Если мы только что переключились на окно Дума
        if (last_focus != app_win) {
            // Вычищаем буфер полностью, чтобы старые Enter-ы не срабатывали
            while(keyboard_pop() != 0); 
            last_focus = app_win;
            regs->rax = 0;
            break;
        }
        regs->rax = keyboard_pop();
    } else {
        last_focus = focused_window;
        regs->rax = 0; 
    }
    break;
}

  case 10: // SYS_EXIT
    term_print("[SYS] Killing process...\n");
    
    // 1. Помечаем задачу как неактивную
    current_task->running = false; 
    
    // 2. Убираем окно приложения
    if (app_win) app_win->active = false;
    if (focused_window == app_win) focused_window = NULL;
    
    // 3. Сбрасываем глобальный флаг (чтобы можно было запустить снова)
    ac97_stop(); 
    extern bool is_app_running;
    is_app_running = false;

    // 4. Срочно переключаем контекст на другую задачу (ядро), 
    // чтобы этот процесс больше не выполнил ни одной инструкции
    yield(); 
    break;
  case 11: // SYS_YIELD (Уступить процессор)
    break;
  case 12: { // SYS_GET_FONT
    extern void *vesa_get_font();
    void *kfont = vesa_get_font();

    uint64_t font_addr = (uint64_t)kfont;
    if (font_addr < hhdm_offset) {
      font_addr = VIRT(font_addr);
    }

    regs->rax = copy_to_user((void *)font_addr, 4096);
    break;
  }
  case 13: { // SYS_SLEEP
    uint32_t ms = regs->rdi;
    uint32_t start = tick * 10;

    __asm__ volatile("sti"); // Включаем аппаратные прерывания!

    // Спим, пока не пройдет нужное время.
    // hlt усыпляет процессор до следующего тика таймера (экономим 100% CPU)
    while ((tick * 10) < start + ms) {
      __asm__ volatile("hlt");
    }

    __asm__ volatile("cli"); // Выключаем обратно перед выходом
    break;
  }

  case 14: {
    uint64_t len = regs->rsi;
    if (len == 0) { regs->rax = 0; break; }
    uint64_t pages = (len + 4095) / 4096;
    void *phys = pmm_alloc_continuous(pages);
    if (!phys) { regs->rax = 0; break; }
    static uint64_t mmap_ptr = 0x700000000000;
    uint64_t virt = mmap_ptr;
    mmap_ptr += (pages * 4096);
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    for (uint64_t i = 0; i < pages; i++) {
      vmm_map((page_table_t *)VIRT(cr3), virt + (i * 4096), (uint64_t)phys + (i * 4096), PTE_USER | PTE_WRITABLE);
    }
    regs->rax = virt;
    break;
  }
  case 15: { // SYS_BRK
    if (current_task->brk == 0) {
        current_task->brk = 0x40000000; 
    }

    uint64_t requested_brk = regs->rdi;
    if (requested_brk == 0) {
        regs->rax = current_task->brk;
        break;
    }

    if (requested_brk > current_task->brk) {
        // ИСПРАВЛЕНИЕ ВЕКА: Добавляем 4095 перед маскированием!
        // Это гарантирует, что мы не перепишем уже замапленную страницу.
        uint64_t start_page = (current_task->brk + 4095) & ~4095;
        uint64_t end_page = (requested_brk + 4095) & ~4095;
        
        uint64_t cr3_val;
        __asm__ volatile("mov %%cr3, %0" : "=r"(cr3_val));
        page_table_t* pml4 = (page_table_t*)VIRT(cr3_val);

        for (uint64_t addr = start_page; addr < end_page; addr += 4096) {
            void* phys = pmm_alloc();
            if (phys) {
                vmm_map(pml4, addr, (uint64_t)phys, PTE_PRESENT | PTE_USER | PTE_WRITABLE);
            }
        }
    }
    current_task->brk = requested_brk;
    regs->rax = current_task->brk;
    break;
}
  case 16: {
    if (regs->rdi == 1 || regs->rdi == 2) {
      term_print((const char *)regs->rsi);
      regs->rax = regs->rdx;
    } else { regs->rax = -1; }
    break;
  }
  case 17: { regs->rax = 0; break; }
  case 18: { regs->rax = -1; break; }
  case 19: { regs->rax = 0; break; }
  case 20: { // SYS_AUDIO_PLAY
    uintptr_t user_ptr = regs->rdi;
    uint32_t size = (uint32_t)regs->rsi;

    static void* s_bufs_phys[32] = {NULL};
    static void* s_bufs_virt[32] = {NULL};
    static int ring_ptr = 0;

    // Инициализируем 32 буфера, чтобы они точно совпадали со слотами AC97
    if (!s_bufs_phys[0]) {
        for(int i=0; i<32; i++) {
            s_bufs_phys[i] = pmm_alloc_continuous(2); // 8KB на буфер
            s_bufs_virt[i] = (void*)((uintptr_t)s_bufs_phys[i] + hhdm_offset);
        }
    }

    uint32_t to_copy = (size > 8192) ? 8192 : size;
    
    // Копируем звук в текущий свободный буфер
    memcpy(s_bufs_virt[ring_ptr], (void*)user_ptr, to_copy);
    
    // Передаем в драйвер конкретный индекс
    extern void ac97_play_at_idx(int idx, void* phys_addr, uint32_t len);
    ac97_play_at_idx(ring_ptr, s_bufs_phys[ring_ptr], to_copy);
    
    // Двигаем указатель по кольцу (0-31)
    ring_ptr = (ring_ptr + 1) % 32;
    break;
  }
  default:
    break;
  }
}