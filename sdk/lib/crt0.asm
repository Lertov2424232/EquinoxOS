[bits 64]
[extern main]
[extern exit]
[global _start]

_start:
    ; 1. Выравниваем стек по 16 байтам (требование ABI)
    and rsp, -16
    
    ; 2. Вызываем конструкторы (если они есть)
    ; Если в Doom нет глобальных объектов C++, эти секции будут пустыми, 
    ; но для совместимости с SDK лучше оставить заглушки или вызвать libc_init
    
    ; 3. Аргументы уже в rdi (argc) и rsi (argv) от ядра
    
    ; 4. Вызываем main
    call main
    
    ; 5. Выход
    mov rdi, rax
    call exit

    jmp $