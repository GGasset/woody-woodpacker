# Direct x86-64 syscalls

*Study guide for woody_woodpacker (42) — how to talk to the kernel without libc from the stub*

## 1. Why the stub can't use libc

Your stub gets injected as **raw bytes** inside the packed binary; it doesn't go through the linker, has no symbol table, can't resolve `write@plt` or anything like that. If it needs to print to the screen, change memory permissions, or terminate the program, it has to ask the kernel directly via the `syscall` instruction. This is exactly what you already know how to do with `syscall()` in C from the common core, except now you're writing it yourself in assembly.

## 2. The mechanics of the `syscall` instruction

```asm
mov rax, <syscall number>
mov rdi, <arg1>
mov rsi, <arg2>
mov rdx, <arg3>
mov r10, <arg4>   ; note: r10, not rcx (syscall uses rcx internally)
mov r8,  <arg5>
mov r9,  <arg6>
syscall
; result in rax
```

The syscall number (`rax`) is defined in `<asm/unistd_64.h>` (or check `ausyscall x86_64 --dump` / the x86-64 syscall table). The ones you'll almost certainly need:

| Syscall     | Number (x86-64) | Arguments                                              |
|-------------|------------------|-----------------------------------------------------------|
| `write`     | 1                | `rdi`=fd, `rsi`=buf, `rdx`=count                         |
| `mmap`      | 9                | `rdi`=addr, `rsi`=len, `rdx`=prot, `r10`=flags, `r8`=fd, `r9`=offset |
| `mprotect`  | 10               | `rdi`=addr, `rsi`=len, `rdx`=prot                        |
| `munmap`    | 11               | `rdi`=addr, `rsi`=len                                    |
| `exit`      | 60               | `rdi`=exit code                                           |
| `open`      | 2                | `rdi`=path, `rsi`=flags, `rdx`=mode                      |
| `read`      | 0                | `rdi`=fd, `rsi`=buf, `rdx`=count                         |
| `close`     | 3                | `rdi`=fd                                                  |

## 3. Return convention and error handling

`rax` holds the result. If it's negative (interpreted as a signed integer, typically between -1 and -4095), it's a negated error code (`-errno`), the same way libc does internally before setting the global `errno` variable. In raw asm you don't have `errno`; if you need to check for an error, compare `rax` against 0 and branch manually.

## 4. `write` — printing `....WOODY....\n` without libc

```asm
; rdi = 1 (stdout), rsi = pointer to string, rdx = length
section .rodata
msg: db "....WOODY....", 0x0a
msg_len: equ $ - msg

section .text
mov rax, 1        ; write syscall
mov rdi, 1         ; fd = stdout
lea rsi, [rel msg] ; string address (rip-relative, important if the binary is PIE)
mov rdx, msg_len
syscall
```

Use `rip`-relative addressing (`lea reg, [rel label]`) so it works no matter where the kernel loaded your segment — essential if the resulting binary is PIE or if you don't control the final load address.

## 5. `mprotect` — the key piece to decrypt in place

The segment where your encrypted code lives probably has `R-X` permissions (read + execute, no write — that's normal for a code segment, and it's good security practice for the kernel to hand it to you that way). To be able to overwrite those bytes with the decrypted version, you need to temporarily request write permission:

```asm
; mprotect(addr, len, PROT_READ|PROT_WRITE|PROT_EXEC)
mov rax, 10          ; mprotect syscall
mov rdi, <addr>       ; must be page-aligned
mov rsi, <len>
mov rdx, 7            ; PROT_READ(1) | PROT_WRITE(2) | PROT_EXEC(4)
syscall
```

⚠️ `addr` must be aligned to the page size (usually 0x1000) — if your encrypted segment doesn't start exactly on a page boundary, you need to round `addr` down to the previous page multiple and adjust `len` accordingly to cover the whole range.

Typical stub flow:
1. `mprotect` to add `PROT_WRITE` to the encrypted region.
2. Decryption loop (read encrypted byte/block, apply the inverse algorithm, write the result back in place).
3. `mprotect` again to restore permissions to what they'd be on the original binary (typically removing `PROT_WRITE`, leaving just `R-X`) — not strictly required for it to work, but more correct and more defensible.
4. `write` the `....WOODY....` banner (you can do this before decrypting, as the subject asks, to signal "this is encrypted" even though you're about to decrypt it right there).
5. `jmp` to the original entry point.

## 6. Decryption loop — example with simple XOR (adapt to whatever real algorithm you choose)

```asm
; rdi = pointer to data, rcx = length, the key is in some known buffer
decrypt_loop:
    mov al, [rdi]
    xor al, <corresponding key byte>
    mov [rdi], al
    inc rdi
    loop decrypt_loop   ; decrements rcx and jumps while rcx != 0
```

This is purely illustrative of the mechanics (read, transform, write, advance pointer). Remember the subject asks for something more sophisticated than a fixed XOR/ROT for it to count as an "advanced algorithm" during evaluation — but the loop structure in asm is the same regardless of the chosen algorithm.

## 7. `exit` — terminating without going through libc (useful while testing)

```asm
mov rax, 60   ; exit syscall
xor rdi, rdi  ; exit code 0
syscall
```

You shouldn't need this in the normal flow (the stub ends with a `jmp` to the original entry point, which eventually calls `exit`), but it's handy to have while debugging the stub in isolation.

## 8. Tools to verify what you generate

```bash
strace ./woody          # see exactly which syscalls it makes at runtime, in order
objdump -d woody         # check the stub assembles to what you think it does
gdb ./woody
(gdb) break *<stub_addr>
(gdb) stepi               # step instruction by instruction inside the stub
(gdb) info registers      # check register values at each step
```

`strace` is especially valuable here: it lets you see the real sequence of `mprotect`/`write`/etc. your packed binary makes, compared to what you expected to write.

## 9. Recommended exercise

Before integrating it into the full packer, write the stub as a **standalone binary** (assemble and link it with `ld`, no libc, with its own `_start`) that does exactly the `mprotect` → "decryption" loop (test first with an identity, no actual encryption) → banner `write` → `exit` sequence. Run it under `strace` and check the syscalls and their arguments match what you expected. Only once this works in isolation does it make sense to inject it inside the real ELF.
