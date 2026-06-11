#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
#
# smoke.sh - boot the kernel in QEMU and prove the shell works.
#
# The serial port is a full console, so we pipe a command script into
# QEMU's stdin and grep the transcript for expected output. The kernel
# powers the VM off at the end, so QEMU exits by itself; the timeout is
# only a safety net for hangs.
#
# Usage: sh tests/smoke.sh [path/to/raptor.elf]

set -u

KERNEL="${1:-build/raptor.elf}"
LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT

[ -f "$KERNEL" ] || { echo "smoke: $KERNEL not found (run make first)"; exit 1; }

drive() {
    sleep 3
    for cmd in \
        'uname -a' \
        'ls /etc' \
        'echo smoke test ok > /tmp/probe' \
        'cat /tmp/probe' \
        'grep smoke /tmp/probe' \
        'free' \
        'lscpu' \
        'poweroff'
    do
        printf '%s\n' "$cmd"
        sleep 0.5
    done
    sleep 5
}

drive | timeout 90 qemu-system-i386 \
    -m 128M -kernel "$KERNEL" \
    -display none -serial stdio -monitor none -no-reboot \
    > "$LOG" 2>&1

expect() {
    if ! grep -q "$1" "$LOG"; then
        echo "smoke: FAIL - missing: $1"
        echo "----- transcript -----"
        cat "$LOG"
        exit 1
    fi
}

expect 'Paging enabled'
expect 'ramfs mounted at /'
expect 'root@raptor'
expect 'Raptor 0.2'                 # uname -a
expect 'os-release'                 # ls /etc
expect 'smoke test ok'              # cat after redirection
expect 'Vendor:'                    # lscpu
expect 'Powering off'

echo "smoke: PASS"
