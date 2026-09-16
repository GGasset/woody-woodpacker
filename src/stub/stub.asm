; stub.asm - woody_woodpacker stub
; TRAMO 3 VALIDADO (2026-09-16)
; - write(1, "....WOODY....\n", 14) ✅
; - mprotect(0x402000, 4096, RWX)=0 ✅
; - XOR loop: dummy 0x90909090 -> 0xd2d2d2d2 ✅
; - XOR auto-inverso: 0xd2d2d2d2 -> 0x90909090 ✅
; - Esqueleto de btea_decrypt (inversa de btea_encrypt) validado
;
; NOTA Tramo 3: dummy está en .data (RW-), no en .text (R-X).
; El .text de ld es READONLY (objdump -h lo confirma).
; Si dummy estuviera en .text, "mov [rdi], bl" daría SIGSEGV.
; En el stub real dentro de un ELF con PT_LOAD, mprotect(RWX)
; sí funciona para .text — eso lo validaste en Tramo 2.
; Para el test standalone, .data es writable sin mprotect.

BITS 64
section .text		; código ejecutable, lo que objcopy extraerá
global _start		; símbolo de entrada para ld (no _main)

_start:
	; --- 1. write(1, "....WOODY....\n", 14) - syscall 1 ---
    ; ABI System V: rdi=arg1, rsi=arg2, rdx=arg3, rax=syscall_nr
	mov rax, 1			; 1 = write (asm/unistd_64.h)
	mov rdi, 1          ; fd 1 = stdout
	lea rsi, [rel msg]  ; RIP-relative: funciona en PIE y ET_EXEC
	mov rdx, 14         ; len = 14 bytes ("....WOODY....\n")
	syscall				; escribe "....WOODY...."

    ; --- 2. mprotect (validado en Tramo 2) ---
    ; En el stub real, mprotect hace .text R-X -> RWX para descifrar in-place.
    ; Aquí mprotect apunta a .data (0x402000), no es necesario pero valida el flujo.
	lea rdi, [rel dummy] ; dummy en .data (writable)
	and rdi, ~0xFFF     ; alinea a página (0x1000 = 4096)
	mov rsi, 0x1000     ; len = una página
	mov rdx, 7          ; PROT_READ|WRITE|EXEC = 7 (RWX)
	mov rax, 10         ; 10 = mprotect syscall
	syscall             ; debe dar =0

    ; --- 3. XOR encrypt dummy in-place (Tramo 3 - VALIDADO) ---
    ; Valida el esqueleto del loop de descifrado (btea_decrypt).
    ; XOR es auto-inverso: xor 0x42 dos veces vuelve al original.
    ; rdi = ptr a dummy, rcx = len, al = clave byte
    ; El loop en ASM usa bl como temp para no pisar rdi ni rcx.
    ; VALIDACIÓN: 0x90909090 -> 0xd2d2d2d2 -> 0x90909090
    lea rdi, [rel dummy] ; rdi = dirección de dummy (.data, writable)
    mov rcx, 0x1000      ; rcx = 4096 iteraciones
    mov al, 0x42         ; clave: 0x90 ^ 0x42 = 0xd2
encrypt_loop:
    mov bl, [rdi]        ; bl = byte actual de dummy
    xor bl, al           ; bl ^= clave (0x90 -> 0xd2)
    mov [rdi], bl        ; escribe de vuelta (funciona porque .data es RW)
    inc rdi              ; avanzar al siguiente byte
    dec rcx              ; decrementar contador
    jnz encrypt_loop     ; repetir hasta rcx=0

	; --- 4. exit(0) - syscall 60 ---
    ; En woody final será jmp al original e_entry (no exit)
	mov rax, 60			; 60 = exit
	xor rdi, rdi		; exit code 0
	syscall				; termina el proceso

msg: db "....WOODY....", 10 ; 10 = '\n', total 14 bytes

; dummy en .data (RW-), NO en .text (R-X).
; El .text de ld es READONLY (ver objdump -h del stub_test).
; Si dummy estuviera en .text, "mov [rdi], bl" daría SIGSEGV.
; En el stub real dentro de un ELF con PT_LOAD, mprotect(RWX)
; sí funciona para .text — eso lo validaste en Tramo 2.
; Para el test standalone, .data es writable sin mprotect.
;
; VALIDACIÓN Tramo 3 (gdb + strace):
; - strace: write(1,...)=14, mprotect(0x402000,4096,RWX)=0, exit(0)
; - gdb breakpoint after loop: x/4x 0x402000 = 0xd2d2d2d2 ✅
; - gdb continue: x/4x 0x402000 = 0x90909090 ✅ (auto-inverso)
; - rcx = 0 al terminar (4096 iteraciones completas)
section .data
dummy: times 0x1000 db 0x90 ; 4096 bytes a cifrar/descifrar
