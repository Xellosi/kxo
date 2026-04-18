#!/usr/bin/env bash
#
# Validate C/H files against .clang-format using clang-format-20.
# Exit 0 if clean, 1 if violations found.

set -e

if ! CLANG_FORMAT=$(command -v clang-format-20); then
    echo "[!] clang-format-20 not installed." >&2
    exit 1
fi

tmpf=$(mktemp)
trap 'rm -f "$tmpf"' EXIT

ret=0
while IFS= read -r -d '' f; do
    if ! "$CLANG_FORMAT" "$f" > "$tmpf" 2>&1; then
        echo "clang-format failed on $f:" >&2
        cat "$tmpf" >&2
        ret=1
        continue
    fi
    if ! diff -u "$f" "$tmpf"; then
        echo ""
        ret=1
    fi
done < <(git ls-files -z '*.c' '*.h')

if [ $ret -eq 0 ]; then
    echo "All files pass clang-format-20."
fi
exit $ret
