# PIE, ASLR and addressing

*Study guide for woody_woodpacker (42) — why "the address" is never a fixed number*

## 1. `ET_EXEC` vs `ET_DYN`: two different worlds

The ELF header's `e_type` field tells you which of the two cases you're dealing with:

- **`ET_EXEC`** (non-PIE executable, "fixed address"): every virtual address in the file (each segment's `p_vaddr`, `e_entry`, symbol addresses) is **absolute and real**. The kernel always loads it at exactly those addresses. This is the simpler case to reason about and debug.
- **`ET_DYN`** (PIE — Position Independent Executable, or a shared library): the addresses in the file are **relative to a load base** that the kernel picks at runtime, typically randomized via ASLR (Address Space Layout Randomization). The real address of something in memory is `load_base + p_vaddr_from_file`.

For years now, gcc/clang generate PIE binaries **by default** on most Linux distributions, so you're very likely to run into `ET_DYN` constantly when testing with "normal" binaries, not just in special cases.

## 2. Why this affects your host program (C)

When you parse the ELF and compute "where" your new segment or the original entry point offset will land, **everything you read from the file (`p_vaddr`, `e_entry`) is relative to the base if the binary is PIE**. This doesn't change how you compute your own new addresses (you keep working with file offsets and virtual addresses within the same relative scheme), but it does matter for:

- Never assuming `e_entry` is a "typical" address like `0x400000` — in PIE it's usually a small relative value (e.g., `0x1050`), which only makes sense once added to the real load base at runtime.
- If the binary is `ET_EXEC` instead of `ET_DYN`, the same formulas still work because, mathematically, it's as if the load base were 0 — but check `e_type` explicitly and don't assume either case.

## 3. Why this affects your stub (asm)

The stub gets copied inside the same ELF file, so it benefits from the same automatic relocation the kernel does when loading the whole binary: if it's PIE, both the original code and your stub load together at the same random base, and the *relative* distances between them (offsets) stay the ones you computed in the file. That's why earlier guides insist on:

- `lea reg, [rip + offset]` instead of absolute addresses to access data inside the stub itself (banner string, embedded key).
- A jump back computed as an offset relative to the original `e_entry` (saved as a file address, not a magic number) to return to normal startup.

Do this correctly and your stub works equally well whether the resulting binary ends up loaded at `0x555555554000` or any other random base — you never need to know the real base at any point, only to work in relative terms.

## 4. An important nuance: is `woody` itself PIE or not?

Here's a design decision you must make consciously: when generating `woody`, do you keep the same `e_type` as the original, or force it to something else? The correct choice to satisfy "execution must be identical" is to **preserve the original `e_type`** — if the binary you were given was PIE, `woody` must be too (and behave with real ASLR), and if it was `ET_EXEC`, `woody` must be too. You have no reason to change it, and changing it could subtly alter behavior (for example, some security protections depend on whether a binary is PIE).

## 5. How to check it and test it in practice

```bash
readelf -h binary | grep Type      # EXEC (Executable file) vs DYN (Shared object file... or PIE executable)
file binary                          # also shows this in readable form
```

To observe ASLR in action:

```bash
cat /proc/sys/kernel/randomize_va_space   # 2 = full ASLR enabled (normal on modern Linux)
gdb ./woody
(gdb) run
(gdb) info proc mappings   # see the real load base for this particular run
(gdb) kill
(gdb) run                   # run again...
(gdb) info proc mappings   # ...the base should have changed if it's PIE
```

If you see the load base change between runs and your `woody` keeps working the same in both, that's a good sign your stub's relative addressing is correct. If it crashes in one run but not another, that's an almost certain sign of a hardcoded absolute address somewhere in the stub.

## 6. Quick checklist for debugging PIE-related issues

- [ ] Do you explicitly check `e_type` in your parser, or do you always assume `ET_EXEC`?
- [ ] Do all data references inside the stub use `rip`-relative addressing?
- [ ] Is the jump back to the original entry point computed as an offset within the file/segment, not as a fixed absolute address?
- [ ] Have you explicitly tested at least one PIE binary and one non-PIE binary (you can force non-PIE with `-no-pie` in gcc/clang) to confirm both cases work?

## 7. Recommended exercise

Compile the same `sample.c` from the subject twice: once normally (PIE by default) and once with `-no-pie`. Compare `e_type` and `e_entry` between the two with `readelf -h`. Pack both with your `woody_woodpacker` and check that both still work exactly like their originals — it's the most direct proof that your address handling is genuinely robust, not just "working by luck" for whichever case you've tested the most.
