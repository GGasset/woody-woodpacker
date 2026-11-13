# woody-woodpacker
Create a simple virus that infects and encrypts ELF64 binaries using C

## Estado de entrada del stub

El stub preserva todos los registros generales x86-64, incluido `rdx` (que el
cargador dinámico puede usar para entregar la función de finalización), y
restaura `rsp` y el contenido original de la pila antes de saltar al entry
original. Esto conserva `argc`, `argv`, el entorno, el vector auxiliar y la
alineación con los que fue invocado. Utiliza espacio por debajo de `rsp`
para guardar registros y la dirección de salto; no preserva su contenido previo.

No preserva los flags ni el estado de registros SIMD/FPU. Además, la prueba es
aislada: no demuestra todavía la integración con el empaquetador. Los errores
de `write` y `mprotect`, y la restauración de permisos originales distintos de
R-X, no están gestionados. Como los cambios en el stub modifican sus offsets,
la integración debe obtener de nuevo los offsets de los datos parcheables.

Para comprobar el salto, los registros y la pila en las rutas con y sin
descifrado, ejecutar `make test-stub-entry` (requiere Linux x86-64, Python 3,
NASM y `ld`). La prueba compara las entradas desde `argc` hasta el terminador
del vector auxiliar, con argumentos y variables de entorno de ejemplo; no
compara los bytes de las cadenas u otros datos apuntados por esas entradas.
La ruta de descifrado comprueba que el bloque cambia, no que el resultado BTEA
sea correcto. Para probar una versión anterior del stub se puede ejecutar
`python3 tests/test_stub_entry.py --stub /ruta/al/stub.asm`.

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
