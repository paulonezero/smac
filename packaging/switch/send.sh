#!/bin/bash
# Sends build/switch/opensmac.nro to the Switch over Wi-Fi and prints the game's output.
#   packaging/switch/send.sh [opensmac arguments...]       e.g. send.sh --new --human 1
# On the console: hold R while starting any game (full memory), then press Y in the Homebrew
# Menu (NetLoader). The console's address: SWITCH_IP, else packaging/switch/.switch-ip.
# The output is also kept in work/switch/send.out.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
IP="${SWITCH_IP:-$(cat "$ROOT/packaging/switch/.switch-ip" 2>/dev/null || true)}"
[ -n "$IP" ] || { echo "error: set SWITCH_IP or write the address to packaging/switch/.switch-ip" >&2; exit 1; }
mkdir -p "$ROOT/work/switch"
# -p is relative to sdmc:/switch (the NetLoader prepends it): the program lands in /switch/smac.
python3 -u "$ROOT/packaging/switch/nxlink.py" -a "$IP" -r 60 -p smac/opensmac.nro -s \
  "$ROOT/build/switch/opensmac.nro" -- "$@" 2>&1 | tee "$ROOT/work/switch/send.out"
