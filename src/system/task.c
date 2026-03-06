#include "task.h"
#include "memory.h"

task_t tasks[8]; // Максимум 8 задач
int current_task = 0;
int task_count = 0;

void init_tasks() {
    current_task = 0;
    tasks[0].active = true;
    task_count = 1;
}

void create_task(void (*entry)(void)) {
    // Выделяем 8КБ под стек новой задачи
    uint64_t* stack = (uint64_t*)kmalloc(8192) + 1024;
    
    // Эмулируем состояние стека, как будто прерывание уже произошло
    // (Порядок такой же, как в SAVE_REGS из interrupt.asm)
    // Чтобы задача начала выполняться с `entry`
    *(--stack) = 0x08;      // CS
    *(--stack) = 0x202;     // RFLAGS (прерывания включены)
    *(--stack) = (uint64_t)entry; // RIP
    
    // Сохраняем регистры (все нули для начала)
    for(int i=0; i<15; i++) *(--stack) = 0;
    
    tasks[task_count].rsp = (uint64_t)stack;
    tasks[task_count].active = true;
    task_count++;
}

void schedule(void* frame) {
    // 1. Сохраняем RSP текущей задачи
    tasks[current_task].rsp = (uint64_t)frame;
    
    // 2. Переключаемся на следующую активную
    current_task = (current_task + 1) % task_count;
    
    // 3. Загружаем RSP новой задачи (этот код ниже)
    // Магия происходит в interrupt.asm
}