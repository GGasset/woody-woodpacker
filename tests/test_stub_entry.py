#!/usr/bin/env python3
"""Run the real stub in an ELF fixture, with and without BTEA decryption."""
from pathlib import Path
import argparse
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--stub", type=Path, default=root / "src/stub/stub.asm")
parser.add_argument("--length", type=int, choices=(0, 8), action="append")
args = parser.parse_args()
stub = args.stub.read_text()
patches = {
    "enc_delta:\n    dq 0": "enc_delta:\n    dq encrypted - stub_base",
    "enc_len:\n    dq 0": "enc_len:\n    dq TEST_LEN",
    "entry_delta:\n    dq 0": "entry_delta:\n    dq destination - stub_base",
}
for original, replacement in patches.items():
    if stub.count(original) != 1:
        raise RuntimeError(f"Cannot locate unique placeholder: {original!r}")
    stub = stub.replace(original, replacement)

with tempfile.TemporaryDirectory(prefix="woody-stub-") as tmp:
    tmp = Path(tmp)
    (tmp / "patched_stub.asm").write_text(stub)
    for length in args.length or (0, 8):
        obj, binary = tmp / "fixture.o", tmp / "fixture"
        subprocess.run(["nasm", "-f", "elf64", f"-DTEST_LEN={length}",
                        "-I", f"{tmp}/", str(root / "tests/stub_entry.asm"),
                        "-o", str(obj)], check=True)
        subprocess.run(["ld", str(obj), "-o", str(binary)], check=True)
        result = subprocess.run([str(binary), "first argument", "second"],
                                env={"WOODY_TEST": "environment value", "SECOND": "2"},
                                capture_output=True)
        if result.returncode:
            reason = "rdx changed" if result.returncode == 2 else "state mismatch or crash"
            raise SystemExit(f"stub entry state: FAIL (length {length}): {reason} "
                             f"(exit {result.returncode})")
        if result.stdout != b"....WOODY....\n":
            raise SystemExit(f"unexpected output: {result.stdout!r}")
        print(f"stub entry state: OK (encrypted length {length})")
