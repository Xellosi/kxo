#!/usr/bin/env bash
#
# Ensure all tracked C/H files end with a newline.

set -e

ret=0
while IFS= read -r -d '' f; do
    [ -s "$f" ] || continue
    if [ "$(tail -c 1 "$f" | wc -l)" -eq 0 ]; then
        echo "Missing final newline: $f"
        ret=1
    fi
done < <(git ls-files -z '*.c' '*.h')

if [ $ret -eq 0 ]; then
    echo "All files end with newline."
fi
exit $ret
