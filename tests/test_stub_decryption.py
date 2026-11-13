#!/usr/bin/env python3
"""Build and exercise the actual raw stub without the ELF inserter."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def run(*args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def main():
    os.environ["LC_ALL"] = "C"
    with tempfile.TemporaryDirectory(prefix="woody-stub-") as tmp:
        tmp = Path(tmp)
        obj, blob, btea, harness = (tmp / name for name in
                                    ("stub.o", "stub.bin", "btea.o", "test"))
        run(os.environ.get("NASM", "nasm"), "-f", "elf64",
            str(ROOT / "src/stub/stub.asm"), "-o", str(obj))
        relocations = run("readelf", "-r", str(obj), capture_output=True, text=True).stdout
        if "There are no relocations" not in relocations:
            raise RuntimeError("stub contains relocations; raw extraction is unsafe")
        run("objcopy", "-O", "binary", "-j", ".text", str(obj), str(blob))
        symbols = {}
        for line in run("nm", "-an", str(obj), capture_output=True, text=True).stdout.splitlines():
            parts = line.split()
            if len(parts) == 3:
                symbols[parts[2]] = int(parts[0], 16)
        offsets = [str(symbols[name] - symbols["_start"]) for name in
                   ("stub_key", "enc_delta", "enc_len", "entry_delta")]
        cc = os.environ.get("CC", "cc")
        # btea.c also contains an unrelated example main; discard its section.
        run(cc, "-ffunction-sections", "-Dmain=btea_example_main", "-c",
            str(ROOT / "src/encryption/btea.c"), "-o", str(btea))
        run(cc, "-Wall", "-Wextra", "-Werror", "-Wl,--gc-sections",
            "-I", str(ROOT / "src/encryption"), str(ROOT / "tests/stub_decryption.c"),
            str(btea), "-o", str(harness))
        for n in (2, 3, 16, 1024, 2049, 32767):
            for positive in (0, 1):
                result = subprocess.run(
                    [str(harness), str(blob), str(n), str(positive), *offsets],
                    capture_output=True, timeout=10)
                if result.returncode or result.stdout != b"....WOODY....\n" or result.stderr:
                    raise AssertionError(f"failed n={n}, positive={positive}, "
                                         f"status={result.returncode}: "
                                         f"stdout={result.stdout!r}, stderr={result.stderr!r}")
                print(f"OK: {n} words, {'positive' if positive else 'negative'} deltas")


if __name__ == "__main__":
    main()
