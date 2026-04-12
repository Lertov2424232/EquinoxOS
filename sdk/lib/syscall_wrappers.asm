global printf
printf:
    mov rax, 1      ; SYS_PRINT
    mov rbx, rdi    ; первый аргумент функции (строка) кладется в RDI по стандарту x86_64
    int 0x80        ; Вызов ядра!
    ret