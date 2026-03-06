#ifndef TASK_H
#define TASK_H
#include <stdint.h>

typedef struct {
    uint64_t rsp; // Указатель стека задачи
    bool active;
} task_t;

void init_tasks();
void create_task(void (*entry)(void));
void schedule(void* frame); // Это сердце планировщика

#endif