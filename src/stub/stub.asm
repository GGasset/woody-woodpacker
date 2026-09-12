BITS 64
section .text		; solo código, lo que objcopy extraerá
global _start		; símbolo de entrada para ld (no _main)

_start:
	; --- 1. write(1, "....WOODY....\n", 14) - syscall 1
    ; ABI System V: rdi=arg1, rsi=arg2, rdx=arg3, rax=syscall_nr
	mov rax, 1			; syscall number 1 = write (asm/unistd_64.h)
	mov rdi, 1          ; fd 1
	lea rsi, [rel msg]
	mov rdx, 14
	syscall

    ; --- 2. mprotect (dummy, 0x1000, 7)
	lea rdi, [rel dummy]
	and rdi, ~0xFFF     ;alinea a página
	mov rsi, 0x1000     ; len = una página
	mov rdx, 7          ; RWX
	mov rax, 10         ; 10 = mprotect
	syscall             ; <- Aquí debe dar =0

	; --- 3. exit(0) - syscall 60 - solo para test standalone, en woody final será jmp
	mov rax, 60			; syscall 60 = exit
	xor rdi, rdi		; exit code 0 (más corto que mov rdi, 0)
	syscall				; no ret, el proceso termina aquí

msg: db "....WOODY....", 10 ; 10 = '\n', total 14 bytes

dummy: times 0x1000 db 0x90 ; 4096 nops, ocupa una página entera
