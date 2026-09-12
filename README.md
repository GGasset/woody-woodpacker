# woody-woodpacker
Create a simple virus that infects and encrypts ELF64 binaries using C

## Stub - Progreso

### Tramo 1 - Toolchain (pusheado)
- `src/stub/stub.asm` imprime `....WOODY....` sin libc, PIC (`lea [rel]`)
- Toolchain validado: `nasm -f elf64 -> ld -> objcopy -O binary -j .text`

### Tramo 2 - mprotect (pusheado)
- Añade `mprotect(dummy, 0x1000, PROT_READ|WRITE|EXEC)` alineado a página (`and rdi, ~0xFFF`)
- Valida que el stub puede hacer RW una página `R-X` (preparación para descifrar in-place)

### Probar (desde la raíz)
```bash
make                # genera src/stub/stub.bin y src/stub/stub_test
./src/stub/stub_test
# ....WOODY....
strace ./src/stub/stub_test
# write(1, "....WOODY....\n",14)=14
# mprotect(0x401000, 4096, PROT_READ|PROT_WRITE|PROT_EXEC)=0
# exit(0)
objdump -D -b binary -m i386:x86-64 src/stub/stub.bin  # verificar lea (%rip)
Siguiente - Tramo 3 (en curso)
- Loop XOR sobre dummy para validar esqueleto de descifrado antes de portar BTEA (btea_decrypt)
