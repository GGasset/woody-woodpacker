; stub.asm - self-contained BTEA decrypting stub
;
; This file is extracted as raw bytes from .text. The C packer must patch:
;   STUB_KEY_OFFSET          16 bytes: four uint32_t key words
;   STUB_ENC_DELTA_OFFSET     8 bytes: encrypted_vaddr - stub_vaddr
;   STUB_ENC_LEN_OFFSET       8 bytes: encrypted length in bytes
;   STUB_ENTRY_DELTA_OFFSET   8 bytes: original_entry_vaddr - stub_vaddr
;
; Addresses are stored as deltas from stub_base. This keeps the stub valid
; for both ET_EXEC and PIE/ET_DYN binaries, where the load base may change.
;
; The old standalone fixtures were deliberately removed from the executable
; flow. They were used to validate MX and BTEA before this generic version.
; This final stub is not runnable by itself until the placeholders are patched.

BITS 64
section .text
global _start

stub_base:
_start:
    ; Preserve callee-saved registers. The original program must see them
    ; unchanged when control returns through the final jmp.
    push rbx
    push r12
    push r13
    push r14
    push r15

    ; write(1, "....WOODY....\n", 14)
    ; syscall ABI: rax=syscall number, rdi/rsi/rdx=arguments.
    mov rax, 1
    mov rdi, 1
    lea rsi, [rel woody_msg]
    mov rdx, 14
    syscall

    ; r15 = address of encrypted data at runtime.
    ; C patches a delta, not an absolute address, so PIE remains valid.
    lea r15, [rel stub_base]
    add r15, [rel enc_delta]

    ; r14 = number of uint32_t words. The C side must provide a length
    ; divisible by four; its ELF padding policy belongs to the packer.
    mov r14, [rel enc_len]
    shr r14, 2
    cmp r14, 2
    jb .restore_registers       ; BTEA needs at least two words

    ; mprotect(encrypted_range, RWX)
    ; mprotect needs a page-aligned start and a length covering every page.
    mov rdi, r15                 ; original start
    and rdi, -0x1000             ; page_start = start & ~0xfff
    mov rax, r14                 ; words -> bytes
    shl rax, 2
    mov rdx, r15                 ; end = data_start + data_length
    add rdx, rax
    add rdx, 0xfff
    and rdx, -0x1000             ; rounded end
    sub rdx, rdi                 ; aligned length
    mov rsi, rdx
    mov rdx, 7                   ; PROT_READ | PROT_WRITE | PROT_EXEC
    mov rax, 10                  ; syscall mprotect
    syscall

    ; r8 = key, r13d = rounds, r12d = sum.
    lea r8, [rel stub_key]

    ; rounds = 6 + 52 / n
    mov eax, 52
    xor edx, edx                 ; dividend is edx:eax
    div r14d
    add eax, 6
    mov r13d, eax

    ; sum = rounds * DELTA, modulo 2^32.
    mov eax, 0x9e3779b9         ; DELTA
    imul eax, r13d
    mov r12d, eax

    ; y = v[0] is persistent state in the reference BTEA algorithm.
    mov r11d, [r15]

.outer_round:
    ; e = (sum >> 2) & 3
    mov r10d, r12d
    shr r10d, 2
    and r10d, 3

    ; p = n - 1. The inner loop processes p=n-1 ... 1.
    mov r9, r14
    dec r9

.inner_loop:
    ; z = v[p - 1]. y remains the value from the previous iteration.
    mov ebx, [r15 + r9*4 - 4]

    ; MX = ((z>>5 ^ y<<2) + (y>>3 ^ z<<4))
    ;      ^ ((sum^y) + (key[(p&3)^e] ^ z))
    mov eax, ebx                 ; eax = z
    shr eax, 5
    mov ecx, r11d               ; ecx = y
    shl ecx, 2
    xor eax, ecx                 ; eax = (z>>5) ^ (y<<2)

    mov ecx, r11d               ; ecx = y
    shr ecx, 3
    mov edx, ebx                 ; edx = z
    shl edx, 4
    xor ecx, edx                 ; ecx = (y>>3) ^ (z<<4)
    add eax, ecx                 ; first half

    mov edx, r12d               ; edx = sum
    xor edx, r11d               ; edx = sum ^ y
    mov edi, r9d                ; edi = p
    and edi, 3
    xor edi, r10d               ; edi = (p&3) ^ e
    mov esi, [r8 + rdi*4]       ; esi = key[(p&3)^e]
    xor esi, ebx                ; esi = key[...] ^ z
    add edx, esi                ; second half
    xor eax, edx                 ; eax = MX

    ; y = v[p] -= MX; the new y is used by the next iteration.
    mov ecx, [r15 + r9*4]
    sub ecx, eax
    mov [r15 + r9*4], ecx
    mov r11d, ecx

    dec r9
    jnz .inner_loop

    ; Final element of each round: z=v[n-1], y=v[0] -= MX.
    ; Do not reload y before MX: BTEA intentionally keeps the previous y.
    mov ebx, [r15 + r14*4 - 4]
    xor r9d, r9d                 ; p = 0, so key index is e

    mov eax, ebx
    shr eax, 5
    mov ecx, r11d
    shl ecx, 2
    xor eax, ecx
    mov ecx, r11d
    shr ecx, 3
    mov edx, ebx
    shl edx, 4
    xor ecx, edx
    add eax, ecx

    mov edx, r12d
    xor edx, r11d
    mov edi, r10d               ; (0 & 3) ^ e == e
    mov esi, [r8 + rdi*4]
    xor esi, ebx
    add edx, esi
    xor eax, edx                 ; eax = MX

    mov ecx, [r15]
    sub ecx, eax
    mov [r15], ecx
    mov r11d, ecx

    ; sum -= DELTA; repeat until rounds reaches zero.
    sub r12d, 0x9e3779b9
    dec r13d
    jnz .outer_round

    ; Restore encrypted segment permissions to R-X.
    mov rdi, r15
    and rdi, -0x1000
    mov rax, r14
    shl rax, 2                  ; data length in bytes
    mov rdx, r15
    add rdx, rax
    add rdx, 0xfff
    and rdx, -0x1000
    sub rdx, rdi
    mov rsi, rdx
    mov rdx, 5                  ; PROT_READ | PROT_EXEC
    mov rax, 10                 ; syscall mprotect
    syscall

.restore_registers:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx

    ; Jump, do not call: preserve the original process stack exactly.
    lea rax, [rel stub_base]
    add rax, [rel entry_delta]
    jmp rax

woody_msg: db "....WOODY....", 10

    ; Runtime-patched data. These bytes are part of the extracted shellcode.
    ; C must patch them using the offsets documented below.
    align 8, db 0x90
stub_key:
    times 16 db 0
enc_delta:
    dq 0
enc_len:
    dq 0
entry_delta:
    dq 0

; Patch offsets relative to the first byte of stub.bin:
STUB_KEY_OFFSET        equ stub_key - stub_base
STUB_ENC_DELTA_OFFSET  equ enc_delta - stub_base
STUB_ENC_LEN_OFFSET    equ enc_len - stub_base
STUB_ENTRY_DELTA_OFFSET equ entry_delta - stub_base
STUB_SIZE              equ $ - stub_base

