#!/usr/bin/env bash
# Boots the kernel, injects real PS/2 scancodes through QEMU's monitor
# (via `sendkey`, over a telnet socket reached with bash's built-in
# /dev/tcp — no extra tools required), and asserts that the keyboard
# driver's IRQ1 path actually reads and translates them: this exercises
# the real inb(0x60)/interrupt-dispatch path, not just the standalone
# scancode_to_ascii table checked by keyboard_translation_test.sh.
#
# Skips (rather than fails) if qemu-system-i386 or bash's /dev/tcp
# support isn't available in this environment.

set -u

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

if ! command -v qemu-system-i386 >/dev/null 2>&1; then
    echo "[SKIP] keyboard test — qemu-system-i386 not available in this environment"
    exit 0
fi

make clean >/dev/null
make >/dev/null 2>&1 || { echo "[FAIL] kernel failed to build"; exit 1; }

MONITOR_PORT=45551
LOG="$(mktemp)"

qemu-system-i386 \
    -kernel techuilaguy-os \
    -serial file:"$LOG" \
    -display none \
    -no-reboot \
    -no-shutdown \
    -monitor telnet:127.0.0.1:${MONITOR_PORT},server,nowait \
    &
QEMU_PID=$!

cleanup() {
    kill -9 "$QEMU_PID" >/dev/null 2>&1
    wait "$QEMU_PID" 2>/dev/null
    rm -f "$LOG"
}
trap cleanup EXIT

# Give the monitor socket time to come up before connecting.
CONNECTED=0
for _ in $(seq 1 20); do
    if exec 3<>"/dev/tcp/127.0.0.1/${MONITOR_PORT}" 2>/dev/null; then
        CONNECTED=1
        break
    fi
    sleep 0.3
done

if [ "$CONNECTED" -ne 1 ]; then
    echo "[FAIL] could not connect to the QEMU monitor"
    exit 1
fi

# Drain the monitor's welcome banner, then let the kernel finish
# booting and unmasking IRQ1 before sending keys.
read -t 2 -u 3 _ || true
sleep 2

send_key() {
    printf 'sendkey %s\n' "$1" >&3
    sleep 0.2
}

send_key h
send_key i
send_key ret

sleep 1
exec 3<&-

FAIL=0

if grep -q "^hi$" "$LOG"; then
    echo "[PASS] keyboard: real injected scancodes (h, i, Enter) were read via inb(0x60) and translated to 'hi'"
else
    echo "[FAIL] keyboard: expected translated input 'hi' not found in serial output"
    FAIL=1
fi

exit $FAIL
