# Assembler toolchain and shellcode extraction

*Study guide for woody_woodpacker (42) — from a `.asm` file to raw injectable bytes*

## 1. The concrete problem

Writing the stub in NASM/GAS gives you a source file. But what you need to inject into the ELF is **raw machine bytes** (shellcode), with no ELF headers of its own, no symbols, nothing — just the already-assembled instructions, ready to be copied as-is into your `woody` binary. This guide covers how to get from one to the other reliably.

## 2. Choosing an assembler: NASM vs GAS

- **NASM**: Intel syntax (`mov rax, 1`), more readable for most people, its own macro syntax. Recommended if you don't already have strong GAS experience from the common core.
- **GAS (GNU Assembler, via `as` or inline in C with `__asm__`)**: AT&T syntax (`movq $1, %rax`), integrates more directly into a typical C-project `Makefile` with `cc`, and is what `objdump -d` produces by default (useful for visual comparison).

Either one is fine for the project; pick whichever the person handling the stub is more comfortable with, and stay consistent throughout the project so you don't mix syntax in your documentation.

## 3. Typical pipeline with NASM

```bash
nasm -f elf64 stub.asm -o stub.o     # assemble into a relocatable ELF64 object
ld stub.o -o stub_test                # (optional) link it to test it standalone
objcopy -O binary -j .text stub_test stub.bin   # extract ONLY the .text bytes, no headers
```

`objcopy -O binary` is the key tool: it converts an ELF (which has headers, sections, etc.) into a flat binary dump of one specific section — exactly the bytes you want to copy into your `woody`.

## 4. Equivalent pipeline with GAS

```bash
as stub.s -o stub.o
ld stub.o -o stub_test
objcopy -O binary -j .text stub_test stub.bin
```

Same end result, same use of `objcopy`.

## 5. Verifying the extracted shellcode is correct

Before trusting `stub.bin`, check that disassembling it again gives you what you expected:

```bash
objdump -D -b binary -m i386:x86-64 stub.bin
```

`-b binary` tells `objdump` this isn't an ELF, it's raw bytes; `-m i386:x86-64` forces the architecture so it interprets the bytes correctly. Compare instruction by instruction against your original `.asm`.

## 6. The addressing problem when assembling standalone

When you assemble and link the stub as a standalone program to test it, the linker assigns it a default load address (typically something like `0x400000` for non-PIE `ET_EXEC`). But once you inject it inside `woody`, it'll live at a different address (whichever one you compute when building the new segment). That's why it's critical that **all addressing inside the stub is relative** (`rip`-relative for accessing data like the `....WOODY....` string or the embedded key; dynamically computed addresses for the jump back, not a `jmp` to a hardcoded assembly-time address). If you use `lea reg, [rel label]` correctly, the extracted shellcode works the same regardless of which address you later copy it to — that's exactly what makes it "position-independent."

## 7. How to pass the key/data length to the stub without reassembling

Since the encryption key differs on every run of `woody_woodpacker` (random, via `/dev/urandom`), it makes no sense to hardcode it in the `.asm`. Two common approaches:

- **Placeholder + binary patching**: leave a reserved zone of fixed size in the `.asm` (e.g., 16 zero bytes) at a known position within the already-assembled `stub.bin`, and from the C host program, after assembling once, overwrite those bytes with the real key before injecting the stub on each run. This means: **you assemble the stub only once** (you could even keep `stub.bin` as a pre-generated resource, or generate it as part of `make`), and the C side only patches bytes at known offsets.
- **Documented offset table**: keep a clear record (commented in the `.asm` itself and in your team's documentation) of at which offset inside `stub.bin` each placeholder falls (key, length of the data to decrypt, address of the original entry point), so the C side knows exactly where to write.

This is exactly the "contract between the two sides" we recommended fixing in writing before coding, in the general project guide — here it gets pinned down at the byte level.

## 8. Makefile integration

The subject requires a Makefile with "the appropriate compilation rules" if you do part of the project in assembly. Example rule skeleton (adapt names):

```makefile
STUB_ASM   = stub.asm
STUB_OBJ   = stub.o
STUB_ELF   = stub_test
STUB_BIN   = stub.bin

$(STUB_BIN): $(STUB_ASM)
	nasm -f elf64 $(STUB_ASM) -o $(STUB_OBJ)
	ld $(STUB_OBJ) -o $(STUB_ELF)
	objcopy -O binary -j .text $(STUB_ELF) $(STUB_BIN)

woody_woodpacker: main.c elf_utils.c crypto.c $(STUB_BIN)
	cc -Wall -Wextra -Werror main.c elf_utils.c crypto.c -o woody_woodpacker
```

Ideally, `stub.bin` is generated as part of the build (a plain `make` produces it automatically as a dependency), and your C code then includes it (for example, embedding it with `xxd -i stub.bin > stub_bytes.h` to generate a C byte array, or reading it from disk at runtime — the first option is more robust for submission, since it doesn't depend on the `.bin` file being present on the grader's system).

## 9. Recommended exercise

Before writing the "real" stub, run the full cycle with a trivial stub (e.g., one that just prints "hello" and calls `exit(0)`): assemble it, extract it with `objcopy`, disassemble it back with `objdump -b binary`, and check it matches. Automate this cycle in the Makefile from the start — finding a bug in the build pipeline halfway through the project, once the stub is already complex, is much more costly than finding it with a one-line stub.
