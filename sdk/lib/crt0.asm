[bits 64]
[extern main]
[global _start]

_start:
    ; Тут в будущем можно настроить стек или аргументы
    call main
    
    ; После выхода из main — завершаем процесс
    mov rax, 10 ; SYS_EXIT
    int 0x80

    ; Если ядро не убило нас — вечный цикл
    jmp $