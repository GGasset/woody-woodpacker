# woody-woodpacker
Create a simple virus that infects and encrypts ELF64 binaries using C

## Stub - Tramo 1 (toolchain)

stub imprime `....WOODY....` sin libc, PIC para PIE.

```bash
make                # genera stub/stub.bin
./src/stub/stub_test    # debe imprimir ....WOODY....
objdump -D -b binary -m i386:x86-64 src/stub/stub.bin # verificar lea 0x..(%rip)
strace ./stub/stub_test   # debe mostrar write(1,"....WOODY....\n",14)=14 + exit(0)
```

## Knowledge required
### ELF64 binaries structure

### Encryption algorithms

### Execute arbitrary assembly instructions stored as plain text during runtime


