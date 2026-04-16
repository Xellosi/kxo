#!/bin/bash
# Integration test: load kxo module, validate device I/O, check for kernel errors.
# Requires root. Run via: make check

set -e

KMOD=kxo.ko
DEV=/dev/kxo
SYSFS=/sys/class/kxo/kxo/kxo_state
DURATION=${1:-5}
PASS_COUNT=0
FAIL_COUNT=0
TOTAL=0

GREEN='\033[32m'
RED='\033[31m'
RESET='\033[0m'

pass()
{
    PASS_COUNT=$((PASS_COUNT + 1))
    TOTAL=$((TOTAL + 1))
    printf "Test %-40s[ ${GREEN}OK${RESET} ]\n" "$1"
}

fail()
{
    FAIL_COUNT=$((FAIL_COUNT + 1))
    TOTAL=$((TOTAL + 1))
    printf "Test %-40s[ ${RED}FAIL${RESET} ] %s\n" "$1" "$2"
}

cleanup()
{
    if [ -n "$READER_PID" ] && kill -0 "$READER_PID" 2> /dev/null; then
        kill "$READER_PID" 2> /dev/null
        wait "$READER_PID" 2> /dev/null
    fi
    if lsmod | grep -q '^kxo '; then
        rmmod kxo 2> /dev/null
    fi
}
trap cleanup EXIT

echo "kxo Integration Tests"

if [ "$(id -u)" -ne 0 ]; then
    echo "  SKIP: not root (run with sudo or via 'make check')"
    exit 0
fi

if [ ! -f "$KMOD" ]; then
    fail "kxo.ko exists" "not found"
    exit 1
fi

if lsmod | grep -q '^kxo '; then
    rmmod kxo 2> /dev/null
fi

dmesg -C

# Module loading
if insmod "$KMOD"; then
    pass "insmod"
else
    fail "insmod" "insmod failed"
    exit 1
fi

# Device node
if [ -c "$DEV" ]; then
    pass "chardev_exists"
else
    fail "chardev_exists" "/dev/kxo not found"
fi

# Sysfs
if [ -f "$SYSFS" ] && [ -n "$(cat "$SYSFS")" ]; then
    pass "sysfs_readable"
else
    fail "sysfs_readable" "kxo_state missing or empty"
fi

# Sustained read
cat "$DEV" > /dev/null 2>&1 &
READER_PID=$!
sleep "$DURATION"

if kill -0 "$READER_PID" 2> /dev/null; then
    pass "device_read_${DURATION}s"
    kill "$READER_PID" 2> /dev/null
    wait "$READER_PID" 2> /dev/null || true
    READER_PID=
else
    fail "device_read_${DURATION}s" "reader died"
fi

# Games completed
WIN_COUNT=$(dmesg | grep -c 'win!!!' || true)
if [ "$WIN_COUNT" -gt 0 ]; then
    pass "games_completed (${WIN_COUNT} wins)"
else
    fail "games_completed" "no wins in dmesg"
fi

# Kernel health
BUG_COUNT=$(dmesg | grep -cE '(BUG|scheduling while atomic|leaked atomic|Oops|WARNING.*kxo)' || true)
if [ "$BUG_COUNT" -eq 0 ]; then
    pass "no_kernel_errors"
else
    fail "no_kernel_errors" "${BUG_COUNT} errors"
    dmesg | grep -E '(BUG|scheduling while atomic|leaked atomic|Oops|WARNING.*kxo)' | head -5
fi

# Clean unload
if rmmod kxo 2> /dev/null; then
    pass "rmmod"
else
    fail "rmmod" "unload failed"
fi

# Cleanup verification
if [ ! -e "$DEV" ]; then
    pass "chardev_removed"
else
    fail "chardev_removed" "/dev/kxo still exists"
fi

echo ""
if [ "$FAIL_COUNT" -eq 0 ]; then
    echo "  All $TOTAL integration tests passed"
else
    echo "  Results: $TOTAL tests, $PASS_COUNT passed, $FAIL_COUNT failed"
fi
[ "$FAIL_COUNT" -eq 0 ]
