#!/usr/bin/env bash
#
# Detect banned unsafe functions in C source (excluding test files).
# Matches the pre-commit hook's banned list: gets, sprintf, strcpy.

set -e

ret=0

while IFS= read -r -d '' f; do
    # Use word-boundary-aware patterns:
    #   \bgets\s*\(     -- catches gets( but not fgets( (\b requires word boundary)
    #   \bsprintf\s*\(  -- unbounded sprintf
    #   \bstrcpy\s*\(   -- unsafe strcpy
    output=$(grep -nE '\b(gets|sprintf|strcpy)\s*\(' "$f" || true)
    combined="$output"
    if [ -n "$combined" ]; then
        echo "Banned function in $f:"
        echo "$output"
        echo ""
        ret=1
    fi
done < <(git ls-files -z 'src/*.c' 'user/*.c' 'include/*.h')

if [ $ret -eq 0 ]; then
    echo "No banned functions found."
fi
exit $ret
