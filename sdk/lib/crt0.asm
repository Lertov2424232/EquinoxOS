[bits 64]
[extern main]
[extern exit]
[global _start]

_start:
    ; 1. Выравниваем стек по 16 байтам
    and rsp, -16
    
    ; 2. Подготавливаем "фейковые" argc и argv
    xor rdi, rdi    ; argc = 0
    xor rsi, rsi    ; argv = NULL
    
    ; 3. Вызываем main
    call main
    
    ; 4. Передаем результат main в exit
    mov rdi, rax
    call exit

    jmp $