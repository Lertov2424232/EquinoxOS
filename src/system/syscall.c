#include "../drivers/vga/vesa.h"
#include "../fs/fat32.h"
#include "../gui/gui.h"
#include "../libc/stdio.h"
#include "../libc/string.h"
#include "../drivers/disk/ata.h"
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
  case 2: { // SYS_READ_FILE (Serious & Safe Edition)
    const char* filename = (const char*)regs->rdi;
    uint32_t* out_size_ptr = (uint32_t*)regs->rsi;
    uint32_t size = 0, f_cluster = 0;

    if (!fat32_find_file_info(filename, &size, &f_cluster)) {
        regs->rax = 0; break;
    }
    if (out_size_ptr) *out_size_ptr = size;

    uint32_t pages_needed = (size + 4095) / 4096;
    
    // Пул адресов для загрузки файлов. 
    // Используем уникальный адрес для каждого вызова, чтобы не пересекаться!
    static uint64_t next_file_vaddr = 0xA0000000;
    uint64_t target_virt = next_file_vaddr;
    next_file_vaddr += (pages_needed * 4096); 
    if (next_file_vaddr > 0xB0000000) next_file_vaddr = 0xA0000000; // Цикличный буфер

    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    page_table_t* pml4 = (page_table_t*)VIRT(cr3);

    uint32_t cluster_size = fat32_get_cluster_size();
    uint32_t sectors_per_cluster = fat32_get_sectors_per_cluster();
    uint32_t first_data_sec = fat32_get_first_data_sector();
    uint32_t copied = 0;

    while (f_cluster >= 2 && f_cluster < 0x0FFFFFF8 && copied < size) {
        uint64_t current_virt = target_virt + copied;

        // 1. Мапим страницы для ТЕКУЩЕГО кластера
        for (uint64_t v = current_virt; v < current_virt + cluster_size; v += 4096) {
            uint64_t page_v = v & ~0xFFFULL;
            if (vmm_get_phys(pml4, page_v) == 0) {
                void* p = pmm_alloc();
                memset((void*)VIRT(p), 0, 4096); // Чистим на всякий случай
                vmm_map(pml4, page_v, (uintptr_t)p, PTE_PRESENT | PTE_USER | PTE_WRITABLE);
            }
        }

        // 2. Читаем один кластер. 
        // Если кластер разбит на разные физические страницы, мы должны читать его
        // ПОСЕКТОРНО, чтобы драйвер ATA корректно попадал в виртуальную память.
        uint32_t f_lba = first_data_sec + (f_cluster - 2) * sectors_per_cluster;
        
        // Магия: так как ядро видит всю память процесса в верхнем диапазоне (HHDM),
        // но наш драйвер ATA работает с линейными адресами, мы будем читать по одному сектору
        // в виртуальный адрес текущего процесса. 
        // НО: target_virt — это адрес в Ring 3. Нам нужен его эквивалент в ядре.
        
        for (uint32_t s = 0; s < sectors_per_cluster; s++) {
            uint64_t sector_vaddr = current_virt + (s * 512);
            uint64_t sector_phys = vmm_get_phys(pml4, sector_vaddr);
            
            // Читаем один сектор прямо в физическую память через HHDM-окно
            read_sectors_ata_pio(VIRT(sector_phys), f_lba + s, 1);
        }

        copied += cluster_size;
        f_cluster = fat32_get_next_cluster(f_cluster);
    }

    regs->rax = target_virt;
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
  term_print("[SYS] Killing process and freeing RAM...\n");

  // ВАЖНО: Останавливаем звук ДО очистки памяти,
  // чтобы драйвер не пытался читать из удаленных страниц
  ac97_stop();

  // 1. Освобождаем физическую память процесса!
  // Эту функцию мы написали в прошлом шаге (в vmm.c)
  extern void vmm_destroy_address_space(uint64_t cr3_phys);
  vmm_destroy_address_space(current_task->cr3);

  // 2. Убиваем задачу
  current_task->running = false;

  extern bool is_app_running;
  is_app_running = false;

  if (app_win)
    app_win->active = false;

  yield(); // Уходим в планировщик
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
    static int ring_ptr = -1;

    // Один раз выделяем 32 буфера
    if (!s_bufs_phys[0]) {
        for(int i=0; i<32; i++) {
            s_bufs_phys[i] = pmm_alloc_continuous(2); 
            s_bufs_virt[i] = (void*)((uintptr_t)s_bufs_phys[i] + hhdm_offset);
            memset(s_bufs_virt[i], 0, 8192);
        }
    }

    extern uint8_t ac97_get_civ();
    
    // При самом первом звуке начинаем писать СРАЗУ ЗА текущим указателем карты
    if (ring_ptr == -1) {
        ring_ptr = (ac97_get_civ() + 1) % 32;
    }

    uint8_t civ = ac97_get_civ();
    
    // ВОТ ОН - ИДЕАЛЬНЫЙ ТОРМОЗ ДЛЯ ДУМА (Дистанция)
    // Считаем, на сколько слотов мы убежали вперед от играющего сейчас
    int dist = (ring_ptr - civ + 32) % 32;
    
    // Если мы оторвались больше чем на 3 буфера — Дум должен подождать!
    // Это дает задержку всего 85мс и НАМЕРТВО защищает от "наслаивания"
    while (dist > 3) {
        __asm__ volatile("pause"); // Ждем, пока карта проиграет звук
        civ = ac97_get_civ();
        dist = (ring_ptr - civ + 32) % 32;
    }

    // Копируем звук
    uint32_t to_copy = (size > 8192) ? 8192 : size;
    memset(s_bufs_virt[ring_ptr], 0, 8192); 
    memcpy(s_bufs_virt[ring_ptr], (void*)user_ptr, to_copy);
    
    // Передаем карте ИНДЕКС и РЕАЛЬНЫЙ размер (to_copy)
    extern void ac97_play_at_idx(int idx, void* phys_addr, uint32_t len);
    ac97_play_at_idx(ring_ptr, s_bufs_phys[ring_ptr], to_copy);
    
    // Двигаем указатель дальше по кругу
    ring_ptr = (ring_ptr + 1) % 32;
    break;
}
  default:
    break;
  }
}