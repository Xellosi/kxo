#!/usr/bin/env python3

"""Rewrite HEAD so its SHA-1 starts with a vanity prefix.

Appends invisible whitespace padding (spaces/tabs) to the commit message
and rewrites HEAD via ``git update-ref`` until the resulting commit
object's SHA-1 starts with PREFIX.  Idempotent: no-op if HEAD already
satisfies the prefix.  Invoked from the post-commit hook.

SHA-1 is computed in-process via ``hashlib`` (no subprocess per trial),
so even ~1M candidates complete in well under a second.
"""

import hashlib
import os
import subprocess
import sys
import time

PREFIX = b"0000"
MAX_BITS = 28
RECURSION_GUARD = "KXO_VANITY_REWRITING"


def capture(*args, stdin=None):
    return subprocess.run(args, input=stdin, check=True, capture_output=True).stdout


def skip_in_ci():
    return bool(os.environ.get("CI")) or os.path.isdir("/home/runner/work")


def find_padding(base):
    """Search for trailing padding (spaces/tabs) yielding PREFIX.

    Returns (bits, padding_bytes, attempts) on success, (None, None, N)
    on exhaustion.  Uses incremental SHA-1 to reuse the common prefix
    state across all candidates of a given bit width.
    """
    total_attempts = 0
    for bits in range(1, MAX_BITS + 1):
        size = len(base) + 1 + bits
        header = b"commit " + str(size).encode() + b"\x00"
        base_state = hashlib.sha1(header + base + b"\n")

        limit = 1 << bits
        for n in range(limit):
            total_attempts += 1
            h = base_state.copy()
            pad = bytes(0x20 if (n >> i) & 1 == 0 else 0x09 for i in range(bits))
            h.update(pad)
            if h.hexdigest().encode().startswith(PREFIX):
                return bits, pad, total_attempts
    return None, None, total_attempts


def main():
    if os.environ.get(RECURSION_GUARD) or skip_in_ci():
        return 0

    current = capture("git", "rev-parse", "HEAD").strip().decode()
    if current.encode().startswith(PREFIX):
        return 0

    base = capture("git", "cat-file", "commit", "HEAD")

    start = time.monotonic()
    bits, pad, attempts = find_padding(base)
    elapsed = time.monotonic() - start

    if bits is None:
        sys.stderr.write(
            f"[vanity-hash] Failed to find prefix '{PREFIX.decode()}' "
            f"after {attempts} attempts\n"
        )
        return 1

    candidate = base + b"\n" + pad
    new_sha = (
        capture(
            "git",
            "hash-object",
            "-w",
            "-t",
            "commit",
            "--stdin",
            stdin=candidate,
        )
        .strip()
        .decode()
    )

    env = os.environ.copy()
    env[RECURSION_GUARD] = "1"
    subprocess.run(
        ["git", "update-ref", "HEAD", new_sha, current],
        check=True,
        env=env,
    )

    sys.stderr.write(
        f'HEAD -> {new_sha[:12]} (prefix "{PREFIX.decode()}", '
        f"{attempts} attempts, {elapsed * 1000:.0f} ms)\n"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
