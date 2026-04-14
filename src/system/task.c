#include "task.h"
#include "pmm.h"
#include "memory.h"
#include "../libc/string.h"
#include "../fs/elf.h"
#include "../fs/fat32.h"

static task_t* current_task = NULL;
static task_t* task_list = NULL;
static uint64_t next_pid = 1;
extern uint64_t hhdm_offset;
extern volatile uint32_t tick;


void task_init() {
    // Создаем "задачу" для самого ядра
    current_task = (task_t*)kmalloc(sizeof(task_t));
    current_task->id = next_pid++;
    current_task->running = true;
    current_task->next = current_task;
    task_list = current_task;
}

// Обновленная функция создания задачи
void task_create(void (*entry)(), uint64_t arg1, uint64_t arg2) {
    task_t* new_task = (task_t*)kmalloc(sizeof(task_t));
    uint64_t stack_phys = (uint64_t)pmm_alloc_continuous(4); 
    uint64_t stack_virt = stack_phys + hhdm_offset;
    memset((void*)stack_virt, 0, 16384);

    stack_frame_t* frame = (stack_frame_t*)(stack_virt + 16384 - sizeof(stack_frame_t));
    
    frame->rip = (uint64_t)entry;
    frame->rdi = arg1; // Теперь тут ровный 64-битный адрес для змейки или argc для bmpview!
    frame->rsi = arg2; // argv или NULL
    frame->cs = 0x28;
    frame->ss = 0x30;
    frame->rflags = 0x202;
    frame->rsp = (uint64_t)frame;

    new_task->rsp = (uint64_t)frame;
    new_task->id = next_pid++;
    new_task->running = true;
    new_task->next = task_list->next;
    task_list->next = new_task;
}

// Эту функцию вызывает таймер 100 раз в секунду
uint64_t schedule(uint64_t current_rsp) {
    if (!current_task) return current_rsp;
    tick++; 
    // Сохраняем указатель на стек текущей задачи
    current_task->rsp = current_rsp;

    // Берем следующую задачу
    current_task = current_task->next;

    // Возвращаем ассемблеру указатель на новый стек
    return current_task->rsp;
}

void yield(void) {
    __asm__ volatile ("int $32"); // Вызываем обработчик таймера (IRQ0)
}

bool task_exec(char* full_command) {
    // 1. Разбиваем строку "bmpview.elf --LOGO.BMP" на токены
    int argc = 0;
    char* argv[16]; // Максимум 16 аргументов
    
    char* token = strtok(full_command, " ");
    while (token != NULL && argc < 16) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }

    if (argc == 0) return false;

    // 2. Читаем ELF (первый токен - это имя файла)
    uint32_t file_size = 0;
    uint8_t* elf_raw = fat32_read_file(argv[0], &file_size);
    if (!elf_raw) return false;

    Elf64_Ehdr* header = (Elf64_Ehdr*)elf_raw;

    // 1. Проверка магии ELF (0x7F 'E' 'L' 'F')
    if (header->e_ident[0] != 0x7F || header->e_ident[1] != 'E' || 
        header->e_ident[2] != 'L' || header->e_ident[3] != 'F') {
        term_print("EXEC: Not a valid ELF file!\n");
        kfree(elf_raw);
        return false;
    }

    // 2. Проходим по программным заголовкам и копируем сегменты
    Elf64_Phdr* phdr = (Elf64_Phdr*)(elf_raw + header->e_phoff);
    for (int i = 0; i < header->e_phnum; i++) {
        if (phdr[i].p_type == 1) { // PT_LOAD (Загружаемый сегмент)
            
            // В идеале тут нужно выделять страницы через PMM, 
            // но так как у тебя приложения линкуются на 0x1000000+, 
            // мы просто копируем их туда (убедись, что ядро там не лежит!)
            
            void* segment_ptr = (void*)(phdr[i].p_vaddr);
            
            // Очищаем память под сегмент (важно для .bss)
            memset(segment_ptr, 0, phdr[i].p_memsz);
            
            // Копируем данные из файла в память
            memcpy(segment_ptr, elf_raw + phdr[i].p_offset, phdr[i].p_filesz);
            
            term_print("EXEC: Loaded segment to 0x");
            // Тут можно вывести адрес для отладки
        }
    }
    char** argv_ptr = kmalloc(sizeof(char*) * argc);
    for(int i = 0; i < argc; i++) {
        argv_ptr[i] = kmalloc(strlen(argv[i]) + 1);
        strcpy(argv_ptr[i], argv[i]);
    }
    // 3. Создаем задачу, точка входа берется из ELF
    term_print("EXEC: Jumping to entry point...\n");
    task_create((void(*)())header->e_entry, argc, argv_ptr);
    
    kfree(elf_raw);
    return true;
}