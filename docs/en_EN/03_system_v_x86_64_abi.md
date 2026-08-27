# System V x86-64 ABI

*Study guide for woody_woodpacker (42) — essential so the stub doesn't break anything when jumping back*

## 1. What an ABI is and why it's critical here

An ABI (Application Binary Interface) is the "contract" that defines how code/functions communicate at the binary level: which registers hold arguments, who must preserve which register, how the stack is aligned. In a normal C project, the compiler respects it for you without you thinking about it. **In woody_woodpacker it doesn't: your stub is hand-written raw asm inserted by hand, so if you violate the ABI, the original program may behave differently or crash after your stub hands control back** — and the subject is explicit that execution must be identical.

## 2. Argument-passing convention (for function calls / syscalls)

The first 6 integer/pointer arguments go in registers, in this order:

| Order | Register (C function) | Register (syscall) |
|-------|------------------------|----------------------|
| 1st   | `rdi`                  | `rdi`                |
| 2nd   | `rsi`                  | `rsi`                |
| 3rd   | `rdx`                  | `rdx`                |
| 4th   | `rcx`                  | `r10` *(watch out, it changes!)* |
| 5th   | `r8`                   | `r8`                 |
| 6th   | `r9`                   | `r9`                 |

The return value goes in `rax`. Additional arguments (7th onward) go on the stack, but you shouldn't need that for your stub.

⚠️ Key difference you can easily forget: when making a **direct syscall** with the `syscall` instruction, the 4th argument goes in `r10`, not `rcx` (because the `syscall` instruction internally uses `rcx` to store the return address). This only matters if your syscall has 4+ arguments (`mmap` does).

## 3. Registers: who preserves them (caller-saved vs callee-saved)

| Type | Registers | What it means |
|------|-----------|----------------|
| **Caller-saved** (volatile) | `rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11` | A function may freely clobber these; if the caller needs them afterward, it must save them itself before the call |
| **Callee-saved** (preserved) | `rbx, rbp, r12, r13, r14, r15` | If a function uses them, it **must** save the original value and restore it before returning |
| Special | `rsp` (stack pointer) | Must end up exactly as it was (or in the state expected by the following code) |

**Direct application to your stub:** when you're done decrypting and about to jump back to the original entry point, any callee-saved register you've touched must be restored to how it was on entry (or, if you're simulating a clean `_start` boot, to the "virgin" state that code expects — normally this isn't an issue because `_start` doesn't expect anything previous, but it does matter if your re-entry point isn't a from-scratch boot). Be especially careful with `rsp`: if you misalign the stack or leave garbage pushed without popping it, the crash can show up instructions (or even functions) after the jump, which is a nightmare to debug.

## 4. Stack alignment

At the entry point of a function (right after the `call`), the ABI requires `rsp % 16 == 0` **before** any `call` executes inside that function (i.e., at the moment of a `call`, `rsp` must be at a 16-byte boundary after the return address `push`). This matters if your stub makes any function call (unlikely if you stick to raw syscalls, but if in the bonus you use some libc function, you must respect it or you might crash on SSE instructions libc uses internally, like `movaps`, which requires 16-byte-aligned addresses).

For `_start` specifically: the kernel hands over control with the stack in a specific state (with `argc` on top, followed by `argv[]`, `envp[]`, `auxv`), not like a normal function call. You don't need to touch the stack for any of this — you only need to **not leave it dirty** when you jump back.

## 5. Jumping back to the original entry point: `jmp`, not `call`

A subtle but important detail: to transfer control to the original `_start` from your stub, use `jmp` (direct jump), **not** `call`. A `call` pushes a return address onto the stack that nobody will ever use (because `_start` never does a `ret`, it normally ends in `exit`), leaving the stack with extra garbage relative to what the kernel expects at startup. A `jmp` to the saved original `e_entry` address preserves exactly the stack state the kernel prepared.

## 6. Absolute vs relative addresses — PIE

If the original binary is `ET_DYN` (PIE, Position Independent Executable, the default nowadays with gcc/clang), every address in the file is **relative to a load base** the kernel picks at runtime (ASLR). This affects how you compute the real address of the original entry point and how you write your own stub (use `rip`-relative addressing, the `lea reg, [rip + offset]` instruction, instead of hardcoded absolute addresses). Check `e_type` in the ELF header to know whether you're dealing with `ET_EXEC` (fixed address) or `ET_DYN` (PIE) and adjust your address computations accordingly.

## 7. Recommended exercise

Write out by hand (on paper, together) the instruction sequence that:
1. Saves any callee-saved registers you're going to use in the stub.
2. Runs your decryption loop using only caller-saved registers or registers you restore.
3. Restores everything.
4. Does a `jmp` (not `call`) to the original entry point's address.

Explain to each other why each step is needed. If either of you can't justify a line, that's the line most likely to give you an intermittent, hard-to-reproduce crash a month from now.
