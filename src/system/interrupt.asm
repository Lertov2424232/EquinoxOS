[bits 64]
[extern panic_handler]
[extern keyboard_callback]
[extern mouse_callback]
[extern timer_callback]
[extern schedule]
[extern current_task]
[extern tasks]
[extern schedule]

%macro SAVE_REGS 0
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro RESTORE_REGS 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

; --- СЕКЦИЯ КОДА ---
section .text

%macro ISR_NOERRCODE 1
[global isr%1]
isr%1:
    push qword 0    ; fake error code
    push qword %1   ; номер прерывания
    jmp exception_common
%endmacro

%macro ISR_ERRCODE 1
[global isr%1]
isr%1:
    ; Код ошибки уже в стеке
    push qword %1
    jmp exception_common
%endmacro

; Генерируем 32 исключения (0-31)
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8   ; Double Fault
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_ERRCODE   21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_ERRCODE   29
ISR_ERRCODE   30
ISR_NOERRCODE 31

exception_common:
    SAVE_REGS
    mov rdi, rsp
    call panic_handler
    ; panic_handler не возвращается

[global keyboard_handler]
keyboard_handler:
    SAVE_REGS
    call keyboard_callback
    mov al, 0x20
    out 0x20, al
    RESTORE_REGS
    iretq 

[global timer_handler]
timer_handler:
    SAVE_REGS
    call timer_callback  ; Просто вызываем функцию
    mov al, 0x20
    out 0x20, al
    RESTORE_REGS
    iretq
    
[global mouse_handler]
mouse_handler:
    SAVE_REGS
    call mouse_callback
    mov al, 0x20
    out 0xA0, al
    out 0x20, al
    RESTORE_REGS
    iretq

[global irq0_handler_asm]
irq0_handler_asm:
    ; 1. Сохраняем все регистры общего назначения
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; 2. Передаем текущий RSP (указатель на этот кадр) в функцию schedule
    mov rdi, rsp
    call schedule
    
    ; 3. Функция schedule вернула нам RSP новой задачи в RAX
    mov rsp, rax

    ; 4. Сигнализируем PIC о конце прерывания (EOI)
    mov al, 0x20
    out 0x20, al

    ; 5. Восстанавливаем регистры новой задачи
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; 6. Возвращаемся из прерывания (процессор сам загрузит RIP, CS, RFLAGS, RSP)
    iretq

; --- СЕКЦИЯ ДАННЫХ ---
section .data
[global isr_stub_table]
isr_stub_table:
%assign i 0
%rep 32
    dq isr%+i
%assign i i+1
%endrep


[global current_task]
[global tasks]
[global schedule]