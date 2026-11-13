#!/bin/bash
# Copies the Switch build and the game's files onto a mounted SD card.
# Dry run by default: lists what would be copied and writes nothing. --apply copies.
#   packaging/switch/make-sd.sh                        # dry run, card found by its atmosphere folder
#   packaging/switch/make-sd.sh --apply
#   SD=/Volumes/NAME packaging/switch/make-sd.sh --apply
# Layout on the card (all under /switch/smac):
#   opensmac.nro   the program
#   game/          the original game's files (game/ here)
#   mods/          files that replace or add to the game's (mods/ here), when there are any
#   user/          settings, saves and the log: made by the game, never touched by this script
# Re-running is safe: rsync skips files already there and unchanged, and deletes nothing.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
APPLY=""
[ "${1:-}" = "--apply" ] && APPLY=1
if [ -z "${SD:-}" ]; then
  found=()
  for v in /Volumes/*; do [ -d "$v/atmosphere" ] && found+=("$v"); done
  [ ${#found[@]} -eq 1 ] || { echo "error: ${#found[@]} Switch SD cards mounted (looked for /Volumes/*/atmosphere). In Hekate: Tools > USB Tools > SD Card, or pass SD=/Volumes/NAME" >&2; exit 1; }
  SD="${found[0]}"
fi
[ -d "$SD" ] || { echo "error: not a folder: $SD" >&2; exit 1; }
NRO="$ROOT/build/switch/opensmac.nro"
GAME="$ROOT/game"
for p in "$NRO" "$GAME/terranx.exe"; do [ -e "$p" ] || { echo "error: missing $p" >&2; exit 1; }; done
DEST="$SD/switch/smac"
# -rt and --modify-window: FAT keeps 2-second times and no owners or permissions.
RSYNC=(rsync -rt --modify-window=2 --itemize-changes --exclude '.DS_Store' --exclude 'README*')
if [ -z "$APPLY" ]; then
  RSYNC+=(--dry-run)
  echo "DRY RUN (nothing written). Add --apply to copy."
else
  mkdir -p "$DEST/game"
fi
echo "SD card: $SD"
echo "== program";    "${RSYNC[@]}" "$NRO" "$DEST/" | tail -2
echo "== game files"; "${RSYNC[@]}" "$GAME/" "$DEST/game/" | wc -l | xargs echo "files to copy:"
if [ -n "$(find "$ROOT/mods" -type f ! -name 'README*' ! -name '.DS_Store' 2>/dev/null | head -1)" ]; then
  [ -n "$APPLY" ] && mkdir -p "$DEST/mods"
  echo "== mods";     "${RSYNC[@]}" "$ROOT/mods/" "$DEST/mods/" | wc -l | xargs echo "files to copy:"
fi
echo "free on card: $(df -h "$SD" | tail -1 | awk '{print $4}')  (the game needs about 700 MB)"
if [ -n "$APPLY" ]; then dot_clean -m "$DEST" 2>/dev/null || true; fi   # macOS ._ files
