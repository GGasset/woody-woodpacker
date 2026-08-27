# How the kernel loads an ELF (`execve`)

*Study guide for woody_woodpacker (42) — why the binary runs the way it does*

## 1. Why you need to understand this

The ELF parser tells you *what's in* the file. This part tells you *what the kernel does with it*, which determines whether your modified binary runs the same as the original or blows up. The rule you'll keep coming back to throughout the project is: **the kernel only looks at the Program Header Table. Sections, symbols, none of that matters to it.**

## 2. The path from `execve` to "first instruction executed"

When someone runs `./woody`:

1. The shell does `fork()` + `execve("./woody", argv, envp)`.
2. The kernel reads the ELF header from the file, validates the magic number and the architecture.
3. The kernel walks the Program Header Table and, **for each `PT_LOAD` entry**, does the equivalent of an `mmap`:
   - Maps `p_filesz` bytes from `p_offset` in the file to virtual address `p_vaddr`.
   - If `p_memsz > p_filesz` (typical in the data segment because of `.bss`), zero-fills the rest up to `p_memsz`.
   - Applies the permissions from `p_flags` (R/W/X) to those pages.
4. If there's a `PT_INTERP` segment, the kernel actually loads **the interpreter** first (usually `/lib64/ld-linux-x86-64.so.2`, the dynamic linker) and hands it control before your `e_entry`. The dynamic linker resolves shared libraries (`libc.so`, etc.) and **then** jumps to `e_entry`.
5. The initial stack is prepared: `argc`, `argv[]`, `envp[]`, and the **auxiliary vector** (`auxv`) — a set of key/value pairs the kernel leaves on the stack with information like `AT_PHDR` (address of the Program Header Table in memory), `AT_ENTRY`, `AT_PAGESZ`, etc. The dynamic linker and libc use these at startup.
6. Finally, the CPU jumps to `e_entry` (or to the interpreter's entry point if there is one), with `rip = e_entry`.

## 3. Direct consequences for your project

- **Only `PT_LOAD` segments matter.** If your stub isn't inside a `PT_LOAD` segment marked executable, the kernel will never map it as executable code, and it won't run (or you'll get a *segfault* on permissions).
- **Page alignment is not optional.** The kernel maps whole pages (typically 4 KB). That's why `p_vaddr` and `p_offset` must match modulo `p_align`: the kernel computes the page-start address from `p_vaddr`, and needs the file offset to "line up" with that.
- **`p_memsz > p_filesz` is zero-fill, not a bug.** If you're extending an existing segment by appending your stub at the end, you have to respect the fact that the "extra" memory part (if any) gets zeroed — you can't just append bytes without thinking about which memory offset they land at.
- **Real R/W/X permissions.** If the segment where you put the encrypted code doesn't have `PF_X`, nothing there will execute (hardware NX protection). If your stub needs to write to a region marked `R-X` only in order to decrypt it in place, **that's exactly why you need `mprotect` at runtime** (covered in the syscalls guide): the kernel gave you those permissions at load time, but you can ask to change them afterwards.
- **If there's a `PT_INTERP` / it's a dynamic binary**, the real execution order is: kernel → dynamic linker → your stub (if you patched `e_entry`) → original binary entry point (`_start`) → `main`. Make sure your "jump back" points at the **original** `e_entry` (before you modified it), not at the interpreter's.

## 4. The "entry point" is not `main`

A common misunderstanding: `e_entry` doesn't point to `main()`, it points to `_start` (a symbol defined by the C runtime, in `crt1.o`), which does libc setup (initializing `argc`/`argv`, registering `__libc_csu_init`, etc.) and **then** calls `main`. This matters because:

- When you save "the original entry point" to jump back to from your stub, you're jumping to `_start`, not `main` — and that's correct, it's exactly what you want: the binary's normal startup happens exactly as if it had never gone through your packer.

## 5. Recommended hands-on exercise

With `gdb` on a simple binary (the `sample` from the subject):

```bash
gdb ./sample
(gdb) info file          # you'll see the sections and their addresses
(gdb) break *<e_entry>   # set a breakpoint at the e_entry address (from readelf -h)
(gdb) run
(gdb) x/5i $pc           # look at the first instructions actually executed
```

Check that the breakpoint's address matches `e_entry` from `readelf -h`, and that the first instructions are libc startup code (`_start`), not your `main`. Repeat the exercise explaining each step to each other — it's the best way to catch a conceptual gap in either of you before it costs you hours of debugging later on.
