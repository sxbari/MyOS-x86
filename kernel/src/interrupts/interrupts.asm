global interrupt_stub
global keyboard_stub
extern timer_handler
extern pic_eoi

extern keyboard_handler
extern scheduler_on_tick
section .text



interrupt_stub:
    ; Save CPU registers
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

    ; Save interrupted task's stack pointer
    mov rbx, rsp

    ; Align stack for C function calls
    and rsp, -16

    ; Update PIT ticks
    call timer_handler

    ; Acknowledge interrupt
    call pic_eoi

    ; Pass interrupted stack pointer
    ; as first argument (RDI)
    mov rdi, rbx

    ; Ask scheduler which task runs next
    call scheduler_on_tick

    ; RAX contains the selected task's RSP
    mov rsp, rax

    ; Restore selected task's registers
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

    ; Resume selected task
    iretq


keyboard_stub:
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

    mov rbx, rsp
    and rsp, -16
    call keyboard_handler
    mov rsp, rbx
    
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

    iretq
