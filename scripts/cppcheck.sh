#!/usr/bin/env bash
#
# Single source of truth for cppcheck configuration.
# Called by 'make cppcheck' (scans all src/, user/, and include/) and the
# pre-commit hook (scans staged C/H files only).
#
# Usage:
#   scripts/cppcheck.sh                 # scan src/*.c user/*.c include/*.h
#   scripts/cppcheck.sh <file> [...]    # scan specific files

set -e

if ! CPPCHECK=$(command -v cppcheck); then
    echo "[!] cppcheck not installed." >&2
    exit 1
fi

# Suppressions chosen to match the project's intent, not to silence real bugs.
# File-wide suppressions are forbidden here; use inline '// cppcheck-suppress X'
# directly above the offending line so the suppression is visible in code review.
SUPPRESSIONS=(
    unmatchedSuppression
    missingIncludeSystem
    variableScope
    checkersReport
    normalCheckLevelMaxBranches
    unusedFunction
    constParameterPointer
    constParameterCallback
    constVariable
    constVariablePointer
    shadowVariable
    unusedStructMember
    staticFunction
    intToPointerCast
)

# Give cppcheck just enough macro knowledge to parse kernel-version guards in
# src/main.c. Without these, cppcheck reports a syntax error at
#   '#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 16, 0)'
# and silently skips the entire translation unit.
DEFINES=(
    -DLINUX_VERSION_CODE=0
    '-DKERNEL_VERSION(a,b,c)=0'
)

OPTS=(-Iinclude -Isrc -Iuser --enable=all --error-exitcode=1 --force --inline-suppr)
OPTS+=("${DEFINES[@]}")
for s in "${SUPPRESSIONS[@]}"; do
    OPTS+=("--suppress=$s")
done

if [ "$#" -eq 0 ]; then
    FILES=(src/*.c user/*.c include/*.h src/rl-private.h)
else
    FILES=("$@")
fi

exec "$CPPCHECK" "${OPTS[@]}" "${FILES[@]}"
