# Encryption algorithms for the packer

*Study guide for woody_woodpacker (42) — choosing and justifying the algorithm*

## 1. What the subject actually requires (and what it doesn't)

The subject says, literally: *"The complexity of your algorithm will nonetheless be a very important part of the grading... An easy ROT isn't considered an advanced algorithm."* It doesn't require a certified standard algorithm, it requires that **you can defend it**: why it's stronger than a trivial XOR, what guarantees it gives and which it doesn't. You don't need to implement full AES — in fact, for a self-decrypting binary that runs once, a complete block cipher with complex modes of operation adds implementation complexity without adding much real value here.

## 2. A baseline to avoid: XOR with a short fixed key

A byte-by-byte XOR with, say, a 4-byte key repeated cyclically is exactly the "easy ROT" the subject rules out: it's trivially reversible via frequency analysis if the attacker knows or suspects patterns in `.text` (x86 instructions have a lot of recognizable structure). Don't use it as your final algorithm, but it's a good first implementation step to validate the injection pipeline before complicating the actual encryption (as in step 5 of the original build plan, "injection test without encryption" — fixed XOR is a useful intermediate stepping stone, not the final result).

## 3. Recommended option: RC4 (or a similar stream cipher with a generated keystream)

RC4 is a good difficulty/time/defensibility sweet spot for this project:
- It's a **stream cipher**: it generates a stream of pseudorandom bytes (the "keystream") from the key, and encrypts/decrypts by XOR-ing the keystream with the data byte by byte. This means encryption and decryption are **the same operation** (practical advantage: a single loop serves both directions).
- The keystream isn't a fixed repeated key like the trivial XOR: each keystream byte depends on a 256-byte internal state mixed via a *Key Scheduling Algorithm* (KSA) and then generated with a *Pseudo-Random Generation Algorithm* (PRGA). This breaks the repetitive patterns that made the simple XOR trivial.
- It's implementable in a handful of lines both in C (for the host side, if you'd rather encrypt there) and in assembly (if you'd rather the stub itself compute it, though for the mandatory part it's enough for the host to encrypt and the stub to decrypt with the same algorithm).

RC4 structure (to study, not to copy without understanding — you'll have to defend it):
1. **KSA**: initialize an array `S[256] = {0, 1, 2, ..., 255}`, then, using the key, shuffle it (a key-dependent pseudorandom permutation).
2. **PRGA**: generate the keystream byte by byte, updating two indices (`i`, `j`) into `S` at each step and using the resulting value to index the keystream byte.
3. **XOR**: `encrypted_byte = original_byte XOR keystream_byte`.

⚠️ Real-world security note (mention it during the defense, it earns credit): RC4 has known cryptographic weaknesses in networking contexts (that's why it's deprecated in TLS), but in this context — encrypting a static blob once with a random, single-use key, with no keystream reuse across different binaries — those weaknesses aren't the relevant weak point; what matters here is not being trivially reversible by inspection, which RC4 clearly satisfies compared to a ROT.

## 4. Alternative: a simplified block cipher (more ambitious)

If you want to go a step further for the mandatory part or as part of your "complexity" justification, you could implement a simplified block cipher (e.g., a few rounds of substitution-permutation over 8- or 16-byte blocks, mixing in the key at each round). It's more work and harder to debug (especially if you do it in the stub in asm), so weigh the time cost/benefit against RC4. It could make sense for the "asm optimization" bonus if you have time left after the mandatory part is solid.

## 5. Key generation: real randomness

The subject asks for the key to be generated "as randomly as possible." **Don't use `rand()`/`srand(time(NULL))`** — it's predictable and low quality. Use the operating system's own generator:

```c
int fd = open("/dev/urandom", O_RDONLY);
read(fd, key_buffer, key_len);
close(fd);
```

`/dev/urandom` gives you bytes with real entropy collected by the kernel, suitable for one-time keys like this one. Remember: the subject asks for the key to be **printed to stdout** when running the main program (`key_value: ...` in the subject's example), typically in hexadecimal.

## 6. Where the key lives at runtime

The decrypting stub needs the key (or the already-derived keystream) available in memory when `woody` runs. Options:
- Embed the key directly in the injected bytes alongside the stub (simpler, expected for the mandatory part).
- (The "parameterized key" bonus) allow the key to be supplied through some external mechanism instead of being embedded fixed — think through the security implications of each approach so you can discuss it during the defense.

## 7. Defense checklist for this part

- [ ] Can you explain, without looking at the code, why your algorithm isn't a ROT in disguise?
- [ ] Do you know what happens if two runs of `woody_woodpacker` on the same binary produce different keys (they should, thanks to `/dev/urandom`'s randomness)?
- [ ] Can you argue how large your key space is and why that matters?
- [ ] Do you know the real weaknesses of your chosen algorithm, and why they aren't critical in this specific usage context?

## 8. Recommended exercise

First implement RC4 (or whichever algorithm you choose) as a standalone C program, completely separate from the project: encrypt a text file, decrypt it, verify `diff` is empty between the original and the encrypt+decrypt result. Only once this is validated in isolation, port the decryption part to assembly for the stub — that way you separate "the algorithm is wrong" bugs from "the ELF injection is wrong" bugs, which are much harder to diagnose if mixed together from the start.
