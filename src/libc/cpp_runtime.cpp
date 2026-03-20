#include <stdint.h>
#include <stddef.h>

// Нужно подключить твой заголовок с kmalloc
extern "C" {
    #include "../system/memory.h"
    #include "../libc/stdio.h"
}

// 1. Операторы выделения памяти
void* operator new(size_t size) {
    return kmalloc(size);
}

void* operator new[](size_t size) {
    return kmalloc(size);
}

// Операторы удаления (пока просто заглушки, если нет kfree)
void operator delete(void* p) { (void)p; }
void operator delete[](void* p) { (void)p; }
void operator delete(void* p, size_t size) { (void)p; (void)size; }
void operator delete[](void* p, size_t size) { (void)p; (void)size; }

// 2. Обработка чисто виртуальных функций
// Вызывается, если кто-то вызвал метод "virtual void func() = 0;"
extern "C" void __cxa_pure_virtual() {
    printf("KERNEL PANIC: Pure virtual function call!");
    while (1) __asm__("hlt");
}

// 3. Секция для вызова конструкторов
typedef void (*constructor)();

extern "C" {
    extern constructor __init_array_start;
    extern constructor __init_array_end;

    void call_global_constructors() {
        for (constructor* i = &__init_array_start; i != &__init_array_end; i++) {
            (*i)();
        }
    }
}

// Заглушка для работы с исключениями (требуется компилятором)
extern "C" void* __dso_handle = (void*) &__dso_handle;
extern "C" int __cxa_atexit(void (*)(void *), void *, void *) { return 0; }