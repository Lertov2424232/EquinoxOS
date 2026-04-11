#include "task.h"
#include "pmm.h"
#include "memory.h"
#include "../libc/string.h"

static task_t* current_task = NULL;
static task_t* task_list = NULL;
static uint64_t next_pid = 1;
extern uint64_t hhdm_offset;


void task_init() {
    // Создаем "задачу" для самого ядра
    current_task = (task_t*)kmalloc(sizeof(task_t));
    current_task->id = next_pid++;
    current_task->running = true;
    current_task->next = current_task;
    task_list = current_task;
}

void task_create(void (*entry)(), void* arg) {
    task_t* new_task = (task_t*)kmalloc(sizeof(task_t));
    
    uint64_t stack_phys = (uint64_t)pmm_alloc_continuous(4); 
    uint64_t stack_virt = stack_phys + hhdm_offset;
    
    memset((void*)stack_virt, 0, 16384);

    stack_frame_t* frame = (stack_frame_t*)(stack_virt + 16384 - sizeof(stack_frame_t));
    
    frame->rip = (uint64_t)entry;
    frame->rdi = (uint64_t)arg; // КРИТИЧНО: передаем указатель на API в приложение
    frame->cs = 0x28;
    frame->ss = 0x30;
    frame->rflags = 0x202; // IF=1 (прерывания разрешены)
    frame->rsp = (uint64_t)frame; // Для корректного восстановления

    new_task->rsp = (uint64_t)frame;
    new_task->id = next_pid++;
    new_task->running = true;

    // Вставка в список
    new_task->next = task_list->next;
    task_list->next = new_task;
}

// Эту функцию вызывает таймер 100 раз в секунду
uint64_t schedule(uint64_t current_rsp) {
    if (!current_task) return current_rsp;

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