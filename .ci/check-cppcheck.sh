#!/usr/bin/env bash
#
# Run cppcheck static analysis via the project's canonical script.
# Wraps scripts/cppcheck.sh with a timeout to prevent CI hangs.

set -e

timeout 120 scripts/cppcheck.sh
