# The ELF format in depth

*Study guide for woody_woodpacker (42) — ELF64 focused*

## 1. What ELF is and why it matters here

ELF (Executable and Linkable Format) is the binary file format used on Linux for:
- Executables (`ET_EXEC`, or `ET_DYN` if it's PIE)
- Relocatable objects (`.o`)
- Shared libraries (`.so`)
- Core dumps

For woody_woodpacker you only care about the **ELF64 executable** case. The central idea to internalize:

> An ELF file has two distinct "views" over the same bytes: the **linking view** (sections, used by the linker and debugging tools) and the **execution view** (segments, used by the kernel during `execve`). You are going to work on the execution view.

## 2. General structure of an ELF64 file

```
+---------------------------+
| ELF Header                |  <- describes the file, points to the rest
+---------------------------+
| Program Header Table      |  <- list of segments (EXECUTION view)
+---------------------------+
| .text, .data, .rodata...  |  <- actual content (code, data)
| .bss, .symtab, .strtab... |
+---------------------------+
| Section Header Table      |  <- list of sections (LINKING view)
+---------------------------+
```

The physical order can vary, but the ELF Header is always at the start (offset 0), and contains the offsets to the other two tables.

## 3. The ELF Header (`Elf64_Ehdr`)

Defined in `<elf.h>`. Key fields:

| Field         | What it is                                                              |
|---------------|---------------------------------------------------------------------------|
| `e_ident[16]` | Magic number `\x7fELF`, class (32/64-bit), endianness, ABI version       |
| `e_type`      | `ET_EXEC` (fixed-address executable) or `ET_DYN` (PIE/shared lib)        |
| `e_machine`   | Architecture, must be `EM_X86_64` (62) — this is your first check         |
| `e_entry`     | **Virtual address of the entry point.** This is what you're going to patch |
| `e_phoff`     | File offset where the Program Header Table starts                        |
| `e_shoff`     | File offset where the Section Header Table starts                        |
| `e_phentsize` / `e_phnum` | Size of each entry / number of program header entries         |
| `e_shentsize` / `e_shnum` | Same, for section headers                                       |
| `e_shstrndx`  | Index of the section that holds the section name strings                 |

**Mandatory check before anything else:** `e_ident[EI_MAG0..3] == "\x7fELF"`, `e_ident[EI_CLASS] == ELFCLASS64`, `e_machine == EM_X86_64`. If any of these fail, print a clean error (no crash).

## 4. Program Header Table — what you'll actually use

Each entry is an `Elf64_Phdr`, and describes a **segment**: a chunk of the file that the kernel must map into memory as-is.

| Field      | What it is                                                                |
|------------|-------------------------------------------------------------------------|
| `p_type`   | `PT_LOAD` (load into memory), `PT_DYNAMIC`, `PT_INTERP`, `PT_NOTE`...    |
| `p_flags`  | Permissions: `PF_R` (4), `PF_W` (2), `PF_X` (1) — combinable            |
| `p_offset` | Offset in the **file** where this segment starts                        |
| `p_vaddr`  | **Virtual** address where it gets mapped in memory                      |
| `p_paddr`  | Physical address (irrelevant for user-space Linux, usually = vaddr)      |
| `p_filesz` | Size of the segment inside the file                                     |
| `p_memsz`  | Size it occupies in memory (can be larger than `p_filesz`, e.g. `.bss`) |
| `p_align`  | Required alignment — **in practice, the page size (0x1000)**            |

Only `PT_LOAD` matters for injecting the stub. You'll typically see:
- A `PT_LOAD` with `PF_R | PF_X` (code segment: `.text`, `.plt`...)
- A `PT_LOAD` with `PF_R | PF_W` (data segment: `.data`, `.bss`...)
- Others like a `PT_LOAD` for `.rodata` on modern binaries with permission separation

### The golden alignment rule

`p_vaddr ≡ p_offset (mod p_align)`. In other words, if your new segment starts at file offset `X`, its virtual address must have the same remainder modulo 0x1000 as `X`. Break this and the resulting binary may fail to load, or load incorrectly — it's the number-one source of "my woody crashes" in this project.

## 5. Two injection strategies (and their trade-offs)

**A. Extend the last `PT_LOAD`**
You append your stub + encrypted data right after the last loadable segment, growing its `p_filesz`/`p_memsz` and (if needed) giving it execute permission. Simpler, but it can collide with the following segment if you don't leave enough room, and if the segment didn't have `PF_X`, you'll need to add it (think through the implications if the binary enforces strict NX).

**B. Add a brand-new `PT_LOAD` segment**
You need a free entry in the Program Header Table, or you'll have to move the whole table somewhere new (e.g. the end of the file) to fit an extra entry. Conceptually cleaner, slightly more rewriting work.

Either approach is valid; document clearly which one you picked and why, for the defense.

## 6. Sections (linking view) — to locate `.text`

Even though *loading* uses segments, figuring out **which bytes to encrypt** (the code, typically `.text`) is easier by looking at the Section Header Table (`Elf64_Shdr`), finding the `.text` section by name (using `e_shstrndx` to resolve the section-name string table).

Relevant fields of `Elf64_Shdr`: `sh_name`, `sh_addr` (virtual address), `sh_offset` (file offset), `sh_size`. With this you know exactly which byte range, both in the file and in memory, corresponds to the code to encrypt.

⚠️ Warning: a *stripped* binary might not have a full Section Header Table. For the mandatory part you can assume non-stripped binaries, but mention it as a known limitation during the defense (or handle it as extra robustness/bonus).

## 7. Tools to practice with (do this before coding)

```bash
readelf -h binary      # ELF header
readelf -l binary      # Program headers (segments)
readelf -S binary      # Section headers
objdump -d binary       # Disassembly
xxd binary | less       # Raw bytes, to verify offsets by hand
```

**Recommended pair exercise:** take the `sample.c` from the subject, compile it, and together fill in by hand (on paper) the values of `e_entry`, all the `p_vaddr`/`p_offset`/`p_filesz`/`p_memsz`/`p_flags` for each `PT_LOAD`, and the `.text` range. If you can both explain that table to each other without looking at `readelf`, you're ready to start coding the parser.
