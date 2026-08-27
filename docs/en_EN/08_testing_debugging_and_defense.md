# Testing, debugging methodology and defense

*Study guide for woody_woodpacker (42) — how to actually know it works, and how to defend it*

## 1. Why this guide exists on top of the others

The earlier guides give you the knowledge to *build* the project. This one gives you the process to *verify* that what you built truly satisfies the subject's toughest requirement: *"in no way is the encrypted program allowed to crash"* and *"execution has to be totally identical to the original binary."* That's not something you check "by eye" by running it once and seeing it prints `Hello, World!` — you need a systematic process.

## 2. Test case matrix

Don't limit yourself to the subject's `sample.c`. Before calling the mandatory part done, test against binaries covering these variables, cross-combined where possible:

| Variable | Cases to cover |
|---|---|
| Binary type | `ET_EXEC` (non-PIE) and `ET_DYN` (PIE) — see the PIE/ASLR guide |
| Linking | Static (`-static`) and dynamic (with shared libc) |
| Code size | A trivial binary (`sample.c`) and a larger one (several functions, loops, its own syscall calls) |
| Runtime behavior | A program that just prints something and exits, and another that reads `argv`/`stdin`, to check the stack and environment arrive intact |
| Arguments to `woody` | None, several, verifying `argc`/`argv` reach it the same as in the original |
| Source compiler | If you have time, test binaries built with both `gcc` and `clang` — small layout differences can reveal incorrect assumptions in your parser |

## 3. Verification protocol per tested binary

For every test binary, follow the same sequence every time and document it (doesn't need to be formal, even a notes file in the repo is fine):

1. **Baseline**: run the original, capture its output and return code (`./original; echo $?`).
2. **Packing**: run `./woody_woodpacker original`, verify it prints the key and there are no errors.
3. **Static verification of the generated `woody`**: `readelf -h woody`, `readelf -l woody` — check `e_entry` points to your stub, that there's a new or extended segment with the expected permissions, and that the Program Header Table is still internally consistent (offsets, alignment).
4. **Running the packed binary**: `./woody`, check it first prints `....WOODY....` and then exactly the same output as the baseline, with the same return code (`echo $?`).
5. **Automated comparison**: `diff <(./original) <(./woody | tail -n +2)` (dropping the banner's first line) to catch any output difference without eyeballing it.
6. **Comparative `strace`**: `strace -f ./original` vs `strace -f ./woody` — the syscalls the program makes **after** your stub hands control back should be identical to the original's (plus your own at the start: `mprotect`, `write` for the banner).

## 4. Debugging when something fails: outside-in

When `woody` crashes or behaves differently, resist the temptation to dive straight into the stub with `gdb` blindly. Go from outside in:

1. **Is it a malformed ELF problem?** `readelf -l woody` — look for broken alignment (`p_vaddr` and `p_offset` not matching modulo `p_align`), overlapping segments, or an `e_entry` that doesn't fall inside any executable `PT_LOAD`.
2. **Is it a problem where the kernel doesn't even reach your stub?** Set a breakpoint directly at the new `e_entry` address (`gdb`, `break *0x...`, taken from `readelf -h woody`) and check that `gdb` actually stops there. If it doesn't, the problem is loading (permissions, offset), not stub logic.
3. **Is it a problem inside the stub?** With the previous breakpoint confirmed, `stepi` instruction by instruction, checking `info registers` at each step against what you expected (especially before and after `mprotect`, and before the final `jmp`).
4. **Is it a problem in the jump back?** Set a second breakpoint at the **original** `e_entry` address (the unpacked binary's) and check you actually get there, with registers and stack in the expected state (see the ABI guide).
5. **Is it a problem after the jump, in already-"normal" code?** If you reach the original entry point fine but the program behaves differently later (e.g., fails to read `argv`), suspect the stack: something you pushed in the stub and didn't pop correctly.

## 5. Mandatory error cases to handle (not just the happy path)

The subject asks for robust error handling ("even in cases of twisted or bad usage"). Minimum list to explicitly test:

- No arguments, or more than one argument.
- File that doesn't exist / no read permission.
- File that exists but isn't an ELF (e.g., a `.txt`).
- Valid ELF but 32-bit (the subject's own example: it should print the unsupported-architecture message, without crashing).
- ELF of a different architecture than x86-64 (ARM, etc., if you have one handy).
- No write permission in the output directory (shouldn't crash when trying to write `woody`, should give a readable error).
- An already-packed binary (what happens if you feed a previously-generated `woody` as input? Not required to support, but it should fail in a controlled way, not with a segfault).

## 6. Defense protocol — what you'll likely be asked

Prepare concrete (not generic) answers to these, together, before the evaluation:

- **"Why this encryption algorithm and not another?"** — have the trivial-XOR-vs-your-choice comparison ready (see the encryption algorithms guide).
- **"Show me in `readelf`/`objdump` exactly what you changed compared to the original binary."** — practice doing this live, pointing out the new/extended segment and the modified `e_entry`.
- **"What happens if...?"** — you'll likely be asked to test some edge case from the list in section 5 live. Have them already tested beforehand, not improvised.
- **"Walk me through your stub instruction by instruction."** — each of you should be able to explain the other's part, not just your own (remember you're a pair, not two independent projects stitched together).
- **"Why do you use `jmp` and not `call` to go back?"** / **"Why `rip`-relative?"** — direct ABI/PIE questions that appear in guides 3 and 7.
- **"What guarantees the resulting binary is byte-for-byte execution-identical to the original after decrypting?"** — you should be able to show the comparative `diff`/`strace` from section 3 live.

## 7. Recommended exercise

A week before the evaluation date, run a "mock defense" between the two of you: one plays grader, the other defends, then swap. Use the questions from section 6 literally. If either of you gets stuck explaining the other's part, that's the signal you need to sit down together and review that part of the code before the real evaluation.
