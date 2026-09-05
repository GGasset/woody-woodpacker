; stub_tramo1.asm
BITS 64
section .text		; solo código, lo que objcopy extraerá
global _start		; símbolo de entrada para ld (no _main)

_start:
	; write(1, "....WOODY....\n", 14) - syscall 1
    ; ABI System V: rdi=arg1, rsi=arg2, rdx=arg3, rax=syscall_nr
	mov rax, 1			; syscall number 1 = write (asm/unistd_64.h)
	lea rsi, [rel msg]	; RIP-relative: funcona en PIE(ET_DYN) y ET_EXEC
	mov rdi, 1			; fd = 1 stdout (ABI: 1er arg en rdi)
	mov rdx, 14			; len = 14 (ABI: 3er arg en rdx)
	syscall				; kernel escribe (rax = bytes escritos o -errno)
	; exit(0) - syscall 60 - solo para test standalone, en woody final será jmp
	mov rax, 60			; syscall 60 = exit
	xor rdi, rdi		; exit code 0 (más corto que mov rdi, 0)
	syscall				; no ret, el proceso termina aquí

msg: db "....WOODY....", 10 ; 10 = '\n', total 14 bytes
