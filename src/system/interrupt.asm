[bits 64]
[extern panic_handler]
[extern keyboard_callback]
[extern mouse_callback]
[extern timer_callback]

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

%macro ISR_NOERRCODE 1
isr%1:
    push qword 0 ; fake err
    push qword %1 ; int no
    jmp exception_common
%endmacro

%macro ISR_ERRCODE 1
isr%1:
    push qword %1 ; int no
    jmp exception_common
%endmacro

ISR_NOERRCODE 1  ; #DB Debug
ISR_NOERRCODE 2  ; Non-Maskable Interrupt
ISR_NOERRCODE 3  ; #BP Breakpoint
ISR_NOERRCODE 4  ; #OF Overflow
ISR_NOERRCODE 5  ; #BR BOUND Range Exceeded
ISR_NOERRCODE 6  ; #UD Invalid Opcode
ISR_NOERRCODE 7  ; #NM Device Not Available
ISR_NOERRCODE 8  ; #DF Double Fault (Has Error Code, usually handled separately)
ISR_NOERRCODE 9  ; Coprocessor Segment Overrun
ISR_NOERRCODE 10 ; #TS Invalid TSS (Has Error Code)
ISR_NOERRCODE 11 ; #NP Segment Not Present (Has Error Code)
ISR_NOERRCODE 12 ; #SS Stack-Segment Fault (Has Error Code)
ISR_NOERRCODE 13 ; #GP General Protection Fault (Has Error Code)
ISR_NOERRCODE 14 ; #PF Page Fault (Has Error Code)
ISR_NOERRCODE 15 ; Reserved
ISR_NOERRCODE 16 ; #MF x87 FPU Floating-Point Error
ISR_NOERRCODE 17 ; #AC Alignment Check (Has Error Code)
ISR_NOERRCODE 18 ; #MC Machine Check
ISR_NOERRCODE 19 ; #XM SIMD Floating-Point Exception
ISR_NOERRCODE 20 ; #VE Virtualization Exception
ISR_NOERRCODE 21 ; Control Protection Exception (Has Error Code)
ISR_NOERRCODE 22 ; Reserved
ISR_NOERRCODE 23 ; Reserved
ISR_NOERRCODE 24 ; Reserved
ISR_NOERRCODE 25 ; Reserved
ISR_NOERRCODE 26 ; Reserved
ISR_NOERRCODE 27 ; Reserved
ISR_NOERRCODE 28 ; Hypervisor Injection Exception
ISR_NOERRCODE 29 ; VMM Communication Exception (Has Error Code)
ISR_NOERRCODE 30 ; Security Exception (Has Error Code)
ISR_NOERRCODE 31 ; Reserved

exception_common:
    SAVE_REGS
    mov rdi, rsp
    call panic_handler
    ; Сюда не возвращаемся

[global isr0]
isr0:
    push qword 0 ; fake err
    push qword 0 ; int no
    SAVE_REGS
    mov rdi, rsp
    call panic_handler
    RESTORE_REGS ; На всякий случай
    add rsp, 16
    iretq

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
    call timer_callback
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