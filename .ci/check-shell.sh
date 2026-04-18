#!/usr/bin/env bash
#
# Validate shell scripts with shfmt (per .editorconfig settings).

set -e

if ! SHFMT=$(command -v shfmt); then
    echo "[!] shfmt not installed." >&2
    exit 1
fi

ret=0
while IFS= read -r -d '' f; do
    [ -f "$f" ] || continue
    if ! "$SHFMT" -d "$f" > /dev/null 2>&1; then
        echo "Shell format violation: $f"
        "$SHFMT" -d "$f" || true
        echo ""
        ret=1
    fi
done < <(git ls-files -z '*.sh' '*.hook' 'scripts/install-git-hooks')

if [ $ret -eq 0 ]; then
    echo "All shell scripts pass shfmt."
fi
exit $ret
