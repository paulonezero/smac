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
OUT="${OUT:-switch}"  # build/$OUT: a second folder keeps a release build beside the development one
docker run --rm -e OUT="$OUT" -v "$ROOT":/w "$IMAGE" bash -c '
  set -e
  [ -f /w/build/$OUT/build.ninja ] || cmake -S /w -B /w/build/$OUT -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake -DCMAKE_BUILD_TYPE=Release \
    -DOPENSMAC_SWITCH_DEV='"${DEV:-OFF}"' > /w/build/$OUT.configure.log 2>&1 \
    || { tail -30 /w/build/$OUT.configure.log; exit 1; }
  ninja -C /w/build/$OUT opensmac_nro > /w/build/$OUT/build.log 2>&1 \
    || { grep -E "error|undefined reference" /w/build/$OUT/build.log | head -40; exit 1; }'
ls -la "$ROOT/build/$OUT/opensmac.nro"
