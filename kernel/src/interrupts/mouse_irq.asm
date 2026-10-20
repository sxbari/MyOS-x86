bits 64
default rel

global mouse_irq_stub

extern mouse_irq_handler

section .text

mouse_irq_stub:

    ; Preserve all general-purpose registers.

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

    ; Save original stack pointer.
    mov rbx, rsp

    ; Align stack before calling C.
    and rsp, -16

    cld
    call mouse_irq_handler

    ; Restore original stack.
    mov rsp, rbx

    ; Restore registers.

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