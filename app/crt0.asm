[bits 64]
extern _start
global _entry        ; <-- ВАЖНО: это точка входа для линковщика

_entry:
    mov rsp, 0x500000   ; Стек на 5 МБ
    call _start         ; Прыгаем в твой код (app.c)
    hlt                 ; Если вернется — стоп