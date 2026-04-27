#include "task.h"
#include "gdt.h"
#include "pmm.h"

extern void term_print(const char* str);
#include "memory.h"
#include "../libc/string.h"
#include "../fs/elf.h"
#include "../fs/fat32.h"
#include "vmm.h"
#include "../libc/stdio.h"

#define IA32_FS_BASE_MSR 0xC0000100
#define IA32_GS_BASE_MSR 0xC0000101

static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

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

    current_task->kstack_at_bottom = (uint64_t)kmalloc(16384) + 16384; 
    
    current_task->next = current_task;
    task_list = current_task;
}

void task_create(void (*entry)(), uint64_t arg1, uint64_t arg2, uint64_t cr3) {
    task_t* new_task = (task_t*)kmalloc(sizeof(task_t));
    memset(new_task, 0, sizeof(task_t)); // ОБЯЗАТЕЛЬНО!
    new_task->brk = 0x40000000;
    new_task->id = next_pid++;
    new_task->running = true;
    new_task->cr3 = cr3;
    new_task->fs_base = 0;

    void* kstack_phys = pmm_alloc_continuous(4); 
    uint64_t kstack_virt = (uint64_t)kstack_phys + hhdm_offset;
    memset((void*)kstack_virt, 0, 16384);
    new_task->kstack_at_bottom = kstack_virt + 16384;

    uint64_t user_stack_virt = 0x70000000000;
    if (cr3 != 0) {
        // Выделяем 512 страниц для стека (2 МБ)
        int stack_pages = 512; 
        void* ustack_phys = pmm_alloc_continuous(stack_pages);
        
        // Мапим ВСЕ страницы, которые выделили
        for (int i = 0; i < stack_pages; i++) { 
            vmm_map((page_table_t*)VIRT(cr3), 
                    user_stack_virt + (i * 4096), 
                    (uint64_t)ustack_phys + (i * 4096), 
                    PTE_USER | PTE_WRITABLE | PTE_PRESENT);
        }
        memset((void*)VIRT(ustack_phys), 0, stack_pages * 4096);

        uint64_t tls_virt = 0x60000000000;
        void* tls_phys = pmm_alloc();
        vmm_map((page_table_t*)VIRT(cr3), tls_virt, (uint64_t)tls_phys, 
                PTE_USER | PTE_WRITABLE | PTE_PRESENT);
        memset((void*)VIRT(tls_phys), 0, 4096);
        
        uint64_t* tcb = (uint64_t*)VIRT(tls_phys);
        tcb[0] = tls_virt + 64;  
        new_task->fs_base = tls_virt;
    }

    stack_frame_t* frame = (stack_frame_t*)(new_task->kstack_at_bottom - sizeof(stack_frame_t));
    memset(frame, 0, sizeof(stack_frame_t));
    
    frame->rip = (uint64_t)entry;
    frame->rdi = arg1; 
    frame->rsi = arg2;
    frame->rflags = 0x202;
    
    if (cr3 == 0) {
        frame->cs = 0x08; 
        frame->ss = 0x10;
        frame->rsp = (uint64_t)kmalloc(16000) + 16000; // Просто пример для ядерных задач
    } else {
        frame->cs = 0x23;
        frame->ss = 0x1B;
        // RSP должен быть выровнен по 16 байт для System V ABI
        frame->rsp = (user_stack_virt + (512 * 4096)) - 16;
    }
    
    new_task->rsp = (uint64_t)frame;
    new_task->next = task_list->next;
    task_list->next = new_task;
}

// task.c
// В task.c
uint64_t schedule(uint64_t current_rsp) {
    tick++;
    if (!current_task) return current_rsp;
    
    current_task->rsp = current_rsp;
    
    do {
        current_task = current_task->next;
    } while (!current_task->running); 

    uint64_t new_cr3 = (current_task->cr3 == 0) ? kernel_cr3 : current_task->cr3;
    __asm__ volatile("mov %0, %%cr3" : : "r"(new_cr3) : "memory");

    gdt_set_tss_stack(current_task->kstack_at_bottom);
    
    if (current_task->fs_base != 0) {
        wrmsr(IA32_FS_BASE_MSR, current_task->fs_base);
    }
    
    return current_task->rsp;
}
void yield(void) {
    __asm__ volatile ("int $32");
}

bool task_exec(char* full_command) {
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

    uint32_t elf_size = 0;
    uint8_t* elf_raw = (uint8_t*)fat32_read_file(argv[0], &elf_size);

    if (!elf_raw) {
        term_print("EXEC: File not found: ");
        term_print(argv[0]);
        term_print("\n");
        kfree(cmd_copy);
        return false;
    }

    Elf64_Ehdr* header = (Elf64_Ehdr*)elf_raw;
    if (memcmp(header->e_ident, "\x7f\x45\x4c\x46", 4) != 0) {
        term_print("EXEC: Not a valid ELF file\n");
        kfree(elf_raw);
        kfree(cmd_copy);
        return false;
    }

    // DEBUG: Dump first 16 bytes of ELF
    term_print("EXEC: ELF Header bytes: ");
    for(int i=0; i<16; i++) {
        char h[4];
        sprintf(h, "%x ", elf_raw[i]);
        term_print(h);
    }
    term_print("\n");
    page_table_t* proc_pml4 = vmm_create_address_space();
    uint64_t phys_pml4 = PHYS(proc_pml4);
    Elf64_Phdr* phdr = (Elf64_Phdr*)(elf_raw + header->e_phoff);
    for (int i = 0; i < header->e_phnum; i++) {
        if (phdr[i].p_type == 1) { // PT_LOAD
            uint64_t vaddr = phdr[i].p_vaddr;
            uint64_t memsz = phdr[i].p_memsz;
            uint64_t filesz = phdr[i].p_filesz;
            uint64_t offset = phdr[i].p_offset;

            // DEBUG: Segment details
            char log[128];
            sprintf(log, "EXEC: Segment %d: vaddr=%x, filesz=%x, memsz=%x, offset=%x\n", i, (uint32_t)vaddr, (uint32_t)filesz, (uint32_t)memsz, (uint32_t)offset);
            term_print(log);

            if (offset + filesz > elf_size) {
                term_print("EXEC: Segment offset/size out of bounds!\n");
                filesz = (offset < elf_size) ? (elf_size - offset) : 0;
            }

            // --- РАСЧЕТ СМЕЩЕНИЙ ---
            uint64_t page_offset = vaddr & 0xFFF;
            uint64_t base_vaddr = vaddr & ~0xFFF;
            uint64_t total_memsz = memsz + page_offset;
            uint64_t num_pages = (total_memsz + 4095) / 4096;

            // 1. Выделяем физическую память под нужной количество страниц
            void* phys_mem = pmm_alloc_continuous(num_pages);
            
            // 2. Мапим страницы в пространство процесса
            for (uint64_t p = 0; p < num_pages; p++) {
                vmm_map(proc_pml4, 
                        base_vaddr + (p * 4096), 
                        (uint64_t)phys_mem + (p * 4096), 
                        PTE_PRESENT | PTE_USER | PTE_WRITABLE);
            }

            // 3. Очищаем ВСЮ выделенную физическую память (HHDM)
            // Это обнуляет .bss автоматически
            memset((void*)(VIRT(phys_mem)), 0, num_pages * 4096);

            // 4. КОПИРУЕМ ДАННЫЕ С УЧЕТОМ СМЕЩЕНИЯ (КРИТИЧНО!)
            // Данные должны лечь по адресу VIRT(phys_mem) + 0x9E0
            memcpy((void*)(VIRT(phys_mem) + page_offset), 
                   elf_raw + offset, 
                   filesz);
            
            term_print("EXEC: Segment loaded correctly.\n");
        }
    }

    term_print("EXEC: Starting Ring 3 process...\n");
    uint64_t user_argv_page = 0xB0000000; 
    void* phys_argv = pmm_alloc();
    vmm_map(proc_pml4, user_argv_page, (uint64_t)phys_argv, PTE_PRESENT | PTE_USER | PTE_WRITABLE);
    
    // ВАЖНО: Обнуляем страницу аргументов!
    memset((void*)VIRT(phys_argv), 0, 4096);

    uint64_t* user_argv_array = (uint64_t*)VIRT(phys_argv); 
    char* user_string_area = (char*)VIRT(phys_argv) + 128; 
    uint64_t current_string_offset = 128;

    for (int i = 0; i < argc; i++) {
        user_argv_array[i] = user_argv_page + current_string_offset;
        strcpy(user_string_area, argv[i]);
    
        int len = strlen(argv[i]) + 1;
        user_string_area += len;
        current_string_offset += len;
    }
    user_argv_array[argc] = 0; 

    term_print("EXEC: Starting Ring 3 process with arguments...\n");
    
    task_create((void(*)())header->e_entry, (uint64_t)argc, user_argv_page, phys_pml4);
    
    // FS base will be set by the scheduler when it switches to this task
    
    kfree(elf_raw);
    kfree(cmd_copy);
    return true;
}