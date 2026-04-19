#include "task.h"
#include "pmm.h"
#include "memory.h"
#include "../libc/string.h"
#include "../fs/elf.h"
#include "../fs/fat32.h"
#include "vmm.h"

static task_t* current_task = NULL;
static task_t* task_list = NULL;
static uint64_t next_pid = 1;
extern uint64_t hhdm_offset;
extern volatile uint32_t tick;


void task_init() {
    current_task = (task_t*)kmalloc(sizeof(task_t));
    current_task->id = next_pid++;
    current_task->running = true;
    current_task->cr3 = 0; // <--- Ядро не меняет CR3
    current_task->next = current_task;
    task_list = current_task;
}
// Обновленная функция создания задачи
void task_create(void (*entry)(), uint64_t arg1, uint64_t arg2, uint64_t cr3) {
    task_t* new_task = (task_t*)kmalloc(sizeof(task_t));
    new_task->id = next_pid++;
    new_task->running = true;
    new_task->cr3 = cr3;

    // Стек ядра (выделяем всегда)
    void* kstack_phys = pmm_alloc_continuous(4); 
    uint64_t kstack_virt = (uint64_t)kstack_phys + hhdm_offset;
    memset((void*)kstack_virt, 0, 16384);
    
    // Запоминаем "дно" стека для TSS.RSP0 (стек растет вниз, так что это адрес + 16кб)
    new_task->kstack_at_bottom = kstack_virt + 16384;

    uint64_t user_stack_virt = 0x70000000000;
    if (cr3 != 0) {
        // Мапим юзер-стек
        void* ustack_phys = pmm_alloc_continuous(4);
        for (int i = 0; i < 4; i++) {
            vmm_map((page_table_t*)VIRT(cr3), 
                    user_stack_virt + (i * 4096), 
                    (uint64_t)ustack_phys + (i * 4096), 
                    PTE_USER | PTE_WRITABLE);
        }
    }

    // Готовим фрейм на стеке ядра
    stack_frame_t* frame = (stack_frame_t*)(new_task->kstack_at_bottom - sizeof(stack_frame_t));
    memset(frame, 0, sizeof(stack_frame_t));
    
    frame->rip = (uint64_t)entry;
    frame->rdi = arg1; 
    frame->rsi = arg2;
    frame->rflags = 0x202;
    
    if (cr3 == 0) {
        frame->cs = 0x08;
        frame->ss = 0x10;
        frame->rsp = kstack_virt + 16000; // Просто чуть выше фрейма
    } else {
        frame->cs = 0x23;
        frame->ss = 0x1B;
        frame->rsp = user_stack_virt + 16384 - 8; // Стек юзера
    }
    
    new_task->rsp = (uint64_t)frame;
    new_task->next = task_list->next;
    task_list->next = new_task;
}

uint64_t schedule(uint64_t current_rsp) {
    if (!current_task) return current_rsp;
    
    current_task->rsp = current_rsp;
    current_task = current_task->next;

    if (current_task->cr3 != 0) {
        __asm__ volatile("mov %0, %%cr3" : : "r"(current_task->cr3));
    }

    // ОЧЕНЬ ВАЖНО: TSS.RSP0 должен указывать на чистый верх стека ядра этой задачи
    gdt_set_tss_stack(current_task->kstack_at_bottom);

    return current_task->rsp;
}

void yield(void) {
    __asm__ volatile ("int $32"); // Вызываем обработчик таймера (IRQ0)
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
    task_create((void(*)())header->e_entry, (uint64_t)argc, 0, phys_pml4);
    
    // 7. Очистка временных буферов
    kfree(elf_raw);
    kfree(cmd_copy);
    return true;
}