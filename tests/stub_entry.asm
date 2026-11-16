BITS 64
section .text
global _start
_start:
    mov [rel saved_rsp], rsp
    ; Find the end of argc, argv, envp and auxv, including AT_NULL's value.
    mov rcx, [rsp]
    lea rsi, [rsp + rcx*8 + 16] ; skip argc, argv and NULL
.env:
    lodsq
    test rax, rax
    jnz .env
.aux:
    lodsq
    add rsi, 8                  ; skip auxv value
    test rax, rax
    jnz .aux
    sub rsi, rsp
    shr rsi, 3
    cmp rsi, 4096
    ja fail
    mov [rel stack_words], rsi
    mov rcx, rsi
    mov rsi, rsp
    lea rdi, [rel saved_stack]
    cld
    rep movsq
    mov rax, 1
    mov rbx, 2
    mov rcx, 3
    mov rdx, 4
    mov rsi, 5
    mov rdi, 6
    mov rbp, 7
    mov r8, 8
    mov r9, 9
    mov r10, 10
    mov r11, 11
    mov r12, 12
    mov r13, 13
    mov r14, 14
    mov r15, 15
    jmp stub_base

destination:
    cmp rsp, [rel saved_rsp]
    jne fail
    cmp rdx, 4
    jne fail_rdx
    cmp rax, 1
    jne fail
    cmp rbx, 2
    jne fail
    cmp rcx, 3
    jne fail
    cmp rsi, 5
    jne fail
    cmp rdi, 6
    jne fail
    cmp rbp, 7
    jne fail
    cmp r8, 8
    jne fail
    cmp r9, 9
    jne fail
    cmp r10, 10
    jne fail
    cmp r11, 11
    jne fail
    cmp r12, 12
    jne fail
    cmp r13, 13
    jne fail
    cmp r14, 14
    jne fail
    cmp r15, 15
    jne fail
    mov rcx, [rel stack_words]
    mov rsi, rsp
    lea rdi, [rel saved_stack]
    cld
    repe cmpsq
    jne fail
%if TEST_LEN == 8
    ; The all-zero ciphertext must have been transformed by BTEA.
    cmp qword [rel encrypted], 0
    je fail
%endif
    xor edi, edi
    jmp exit
fail:
    mov edi, 1
    jmp exit
fail_rdx:
    mov edi, 2
exit:
    mov eax, 60
    syscall

; The generated copy has its placeholders patched with these fixture labels.
%define _start stub_entry
%include "patched_stub.asm"
%undef _start

align 4096, db 0x90
encrypted: dq 0

section .bss
saved_rsp: resq 1
stack_words: resq 1
saved_stack: resq 4096
