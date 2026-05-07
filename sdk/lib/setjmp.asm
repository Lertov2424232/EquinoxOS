; sdk/lib/setjmp.asm
[bits 64]
global setjmp
global longjmp

setjmp:
    ; rdi = jmp_buf
    mov [rdi + 0], rbx
    mov [rdi + 8], rbp
    mov [rdi + 16], r12
    mov [rdi + 24], r13
    mov [rdi + 32], r14
    mov [rdi + 40], r15
    lea rdx, [rsp + 8]      ; Сохраняем стек, какой он был до вызова
    mov [rdi + 48], rdx
    mov rdx, [rsp]          ; Сохраняем адрес возврата (RIP)
    mov [rdi + 56], rdx
    xor rax, rax            ; Возвращаем 0
    ret

longjmp:
    ; rdi = jmp_buf, rsi = value
    mov rbx, [rdi + 0]
    mov rbp, [rdi + 8]
    mov r12, [rdi + 16]
    mov r13, [rdi + 24]
    mov r14, [rdi + 40]
    mov r15, [rdi + 40]
    mov rsp, [rdi + 48]
    mov rdx, [rdi + 56]     ; RIP
    
    mov rax, rsi            ; Возвращаем значение из rsi
    test rax, rax
    jnz .not_zero
    inc rax                 ; longjmp не может возвращать 0
.not_zero:
    jmp rdx                 ; Прыгаем обратно в setjmp