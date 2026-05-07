[bits 64]

section .text
global _syscall
_syscall:
    mov rax, rdi    ; Переносим номер системного вызова в RAX
    mov rdi, rsi    ; Сдвигаем аргументы для ядра
    mov rsi, rdx
    mov rdx, rcx
    mov rcx, r8
    mov r8, r9
    
    int 0x80        ; Летим в ядро!
    
    ret             ; Результат из ядра уже лежит в RAX, просто возвращаемся