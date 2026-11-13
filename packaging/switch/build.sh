#!/bin/bash
# Builds build/switch/opensmac.nro (Nintendo Switch homebrew) in devkitPro's Docker image.
#   packaging/switch/build.sh            a copy that listens on no port
#   DEV=ON packaging/switch/build.sh     with the development loop (dev.py): for your own console only
# (changing DEV needs a fresh configure: remove build/switch first)
# Needs Docker (OrbStack). The image has the compiler, libnx, SDL2, SDL2_ttf and FreeType.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
IMAGE="${IMAGE:-devkitpro/devkita64:20260219}"
docker info >/dev/null 2>&1 || { orb start >/dev/null 2>&1 || true; }
docker info >/dev/null 2>&1 || { echo "error: Docker is not answering" >&2; exit 1; }
docker run --rm -v "$ROOT":/w "$IMAGE" bash -c '
  set -e
  [ -f /w/build/switch/build.ninja ] || cmake -S /w -B /w/build/switch -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake -DCMAKE_BUILD_TYPE=Release \
    -DOPENSMAC_SWITCH_DEV='"${DEV:-OFF}"' > /w/build/switch.configure.log 2>&1 \
    || { tail -30 /w/build/switch.configure.log; exit 1; }
  ninja -C /w/build/switch opensmac_nro > /w/build/switch/build.log 2>&1 \
    || { grep -E "error|undefined reference" /w/build/switch/build.log | head -40; exit 1; }'
ls -la "$ROOT/build/switch/opensmac.nro"
