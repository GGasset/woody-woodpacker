# woody-woodpacker
Create a simple virus that infects and encrypts ELF64 binaries using C

## Stub - Progreso

### Tramo 1 - Toolchain (pusheado)
- `src/stub/stub.asm` imprime `....WOODY....` sin libc, PIC (`lea [rel]`)
- Toolchain validado: `nasm -f elf64 -> ld -> objcopy -O binary -j .text`

### Tramo 2 - mprotect (pusheado)
- Añade `mprotect(dummy, 0x1000, PROT_READ|WRITE|EXEC)` alineado a página (`and rdi, ~0xFFF`)
- Valida que el stub puede hacer RW una página `R-X` (preparación para descifrar in-place)

### Tramo 3 - XOR loop VALIDADO (pusheado)
- Loop XOR in-place sobre `dummy` en `.data` (RW-)
- Valida el esqueleto de `btea_decrypt` (auto-inverso)
- `0x90909090` -> `0xd2d2d2d2` -> `0x90909090` (XOR 0x42 dos veces)
- rcx=0 al terminar (4096 iteraciones)
- El fallo de SIGSEGV de antes era porque `dummy` estaba en `.text` (R-X). Solución: `.data`

### Probar (desde la raíz)
```bash
make                # genera src/stub/stub.bin y src/stub/stub_test
./src/stub/stub_test
# ....WOODY....
strace ./src/stub/stub_test
# write(1, "....WOODY....\n",14)=14
# mprotect(0x402000, 4096, PROT_READ|PROT_WRITE|PROT_EXEC)=0
# exit(0)
objdump -D -b binary -m i386:x86-64 src/stub/stub.bin  # verificar lea (%rip)
gdb ./src/stub/stub_test
# break *0x401053, run, x/4x 0x402000, continue
# 0xd2d2d2d2 -> 0x90909090 (auto-inverso)
```

### Siguiente - Tramo 4+
- Portar `btea_decrypt` a ASM (el mismo esqueleto de loop, solo `xor 0x42` -> `MX`)
- Inyectar stub en ELF con PT_LOAD + `mprotect` real
- Parchear `enc_key`, `enc_addr`, `enc_len`, `orig_entry`

## Knowledge required
### ELF64 binaries structure

### Encryption algorithms

### Execute arbitrary assembly instructions stored as plain text during runtime
