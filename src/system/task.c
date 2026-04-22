#include "task.h"
#include "pmm.h"
#include "memory.h"
#include "../libc/string.h"
#include "../fs/elf.h"
#include "../fs/fat32.h"
#include "vmm.h"

task_t* current_task = NULL;
static task_t* task_list = NULL;
static uint64_t next_pid = 1;
extern uint64_t hhdm_offset;
extern volatile uint32_t tick;
static uint64_t kernel_cr3 = 0;

void task_init() {
    __asm__ volatile("mov %%cr3, %0" : "=r"(kernel_cr3));
    current_task = (task_t*)kmalloc(sizeof(task_t));
    current_task->cr3 = kernel_cr3;
    current_task->id = next_pid++;
    current_task->running = true;

    // КРИТИЧНО: У ПЕРВОЙ задачи ядра тоже должен быть стек для TSS
    current_task->kstack_at_bottom = (uint64_t)kmalloc(16384) + 16384; 
    
    current_task->next = current_task;
    task_list = current_task;
}
// Обновленная функция создания задачи
void task_create(void (*entry)(), uint64_t arg1, uint64_t arg2, uint64_t cr3) {
    task_t* new_task = (task_t*)kmalloc(sizeof(task_t));
    new_task->id = next_pid++;
    new_task->running = true;
    new_task->cr3 = cr3;

    // Стек ядра
    void* kstack_phys = pmm_alloc_continuous(4); 
    uint64_t kstack_virt = (uint64_t)kstack_phys + hhdm_offset;
    memset((void*)kstack_virt, 0, 16384);
    new_task->kstack_at_bottom = kstack_virt + 16384;

    uint64_t user_stack_virt = 0x70000000000;
    if (cr3 != 0) {
        // Мапим 8 страниц стека (32КБ) для надежности
        void* ustack_phys = pmm_alloc_continuous(8);
        for (int i = 0; i < 8; i++) {
            vmm_map((page_table_t*)VIRT(cr3), 
                    user_stack_virt + (i * 4096), 
                    (uint64_t)ustack_phys + (i * 4096), 
                    PTE_USER | PTE_WRITABLE | PTE_PRESENT);
        }
    }

    stack_frame_t* frame = (stack_frame_t*)(new_task->kstack_at_bottom - sizeof(stack_frame_t));
    memset(frame, 0, sizeof(stack_frame_t));
    
    frame->rip = (uint64_t)entry;
    frame->rdi = arg1; 
    frame->rsi = arg2;
    frame->rflags = 0x202; // IF = 1
    
    if (cr3 == 0) {
        frame->cs = 0x08; frame->ss = 0x10;
        frame->rsp = kstack_virt + 16000;
    } else {
        frame->cs = 0x23; // User Code
        frame->ss = 0x1B; // User Data
        // Стек должен быть выровнен по 16 байт - 8 (для соответствия ABI)
        frame->rsp = user_stack_virt + (8 * 4096) - 16; 
    }
    
    new_task->rsp = (uint64_t)frame;
    new_task->next = task_list->next;
    task_list->next = new_task;
}

// task.c
uint64_t schedule(uint64_t current_rsp) {
    if (!current_task) return current_rsp;
    current_task->rsp = current_rsp;
    task_t* next = current_task->next;
    while (next != current_task) {
        if (next->sleep_until <= tick) {
            next->running = true;
            next->sleep_until = 0;
        }
        
        if (next->running) break;
        next = next->next;
    }
    
    current_task = next;

    // Переключаем CR3 только если он отличается (экономит ресурсы TLB)
    uint64_t new_cr3 = (current_task->cr3 == 0) ? kernel_cr3 : current_task->cr3;
    
    uint64_t old_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(old_cr3));
    if (old_cr3 != new_cr3) {
        __asm__ volatile("mov %0, %%cr3" : : "r"(new_cr3) : "memory");
    }

    gdt_set_tss_stack(current_task->kstack_at_bottom);
    return current_task->rsp;
}

void yield(void) {
    __asm__ volatile ("sti; hlt");  // Вызываем обработчик таймера (IRQ0)
}

bool task_exec(char* full_command) {
    // 1. Разбиваем строку на токены (название файла и аргументы)
    int argc = 0;
    char* argv[16]; 
    
    char* cmd_copy = (char*)kmalloc(strlen(full_command) + 1);
    strcpy(cmd_copy, full_command);

    char* token = strtok(cmd_copy, " ");
    while (token != NULL && argc < 16) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }

    if (argc == 0) {
        kfree(cmd_copy);
        return false;
    }

    // 2. ЧИТАЕМ ФАЙЛ С ДИСКА
    uint32_t elf_size = 0;
    // argv[0] — это имя файла (например "SNAKE.ELF")
    uint8_t* elf_raw = (uint8_t*)fat32_read_file(argv[0], &elf_size);

    if (!elf_raw) {
        term_print("EXEC: File not found: ");
        term_print(argv[0]);
        term_print("\n");
        kfree(cmd_copy);
        return false;
    }

    // 3. Проверяем, что это ELF
    Elf64_Ehdr* header = (Elf64_Ehdr*)elf_raw;
    if (memcmp(header->e_ident, "\x7f\x45\x4c\x46", 4) != 0) {
        term_print("EXEC: Not a valid ELF file\n");
        kfree(elf_raw);
        kfree(cmd_copy);
        return false;
    }

    // 4. Создаем адресное пространство (VMM)
    page_table_t* proc_pml4 = vmm_create_address_space();
    uint64_t phys_pml4 = PHYS(proc_pml4);

    // 5. Загружаем сегменты ELF в виртуальную память процесса
    Elf64_Phdr* phdr = (Elf64_Phdr*)(elf_raw + header->e_phoff);
    for (int i = 0; i < header->e_phnum; i++) {
        if (phdr[i].p_type == 1) { // PT_LOAD
            uint64_t pages = (phdr[i].p_memsz + 4095) / 4096;
            void* phys_mem = pmm_alloc_continuous(pages);
            
            // Мапим каждую страницу сегмента как USER
            for (uint64_t p = 0; p < pages; p++) {
                vmm_map(proc_pml4, 
                        phdr[i].p_vaddr + (p * 4096), 
                        (uint64_t)phys_mem + (p * 4096), 
                        PTE_USER | PTE_WRITABLE);
            }
            
            // Копируем данные из ELF в выделенную физическую память через HHDM
            memset((void*)VIRT(phys_mem), 0, phdr[i].p_memsz);
            memcpy((void*)VIRT(phys_mem), elf_raw + phdr[i].p_offset, phdr[i].p_filesz);
        }
    }

    // 6. Создаем задачу
    term_print("EXEC: Starting Ring 3 process...\n");
    // Передаем argc в RDI, argv пока оставим 0
    uint64_t user_argv_page = 0x80000000; 
    void* phys_argv = pmm_alloc();
    vmm_map(proc_pml4, user_argv_page, (uint64_t)phys_argv, PTE_USER | PTE_WRITABLE);
    
    // Формируем структуру в физической памяти (ядро пишет через HHDM)
    uint64_t* user_argv_array = (uint64_t*)VIRT(phys_argv); 
    char* user_string_area = (char*)VIRT(phys_argv) + 128; // Строки положим через 128 байт
    
    uint64_t current_string_offset = 128; // Смещение относительно начала страницы

    // Заполняем массив указателей и копируем сами строки
    for (int i = 0; i < argc; i++) {
        // Записываем УКАЗАТЕЛЬ (виртуальный адрес в памяти юзера!)
        user_argv_array[i] = user_argv_page + current_string_offset;
        
        // Копируем саму строку (через HHDM)
        strcpy(user_string_area, argv[i]);
        
        // Сдвигаем указатели для следующей строки
        int len = strlen(argv[i]) + 1; // +1 для нуль-терминатора
        user_string_area += len;
        current_string_offset += len;
    }
    // Последний элемент массива argv должен быть NULL по стандарту C
    user_argv_array[argc] = 0; 

    term_print("EXEC: Starting Ring 3 process with arguments...\n");
    
    // Передаем argc в RDI (arg1), и адрес МАССИВА в RSI (arg2)
    task_create((void(*)())header->e_entry, (uint64_t)argc, user_argv_page, phys_pml4);
    
    // 7. Очистка временных буферов
    kfree(elf_raw);
    kfree(cmd_copy);
    return true;
}