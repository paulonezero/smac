#!/bin/bash
# Builds one self-contained macOS application: OpenSMAC, its SDL2 libraries and the game's files
# (game/) in a single .app that needs nothing else on the machine.
#
#   packaging/package_app.sh            dry run: says what it would do, writes nothing
#   packaging/package_app.sh --apply    builds build/app/Alpha Centauri.app
#
# Options: --out DIR    where the app goes (default build/app)
#          --name NAME  the app's name (default "Alpha Centauri")
#
# PRIVATE USE ONLY: the app contains the copyrighted game. Unlike packaging/package.sh (the
# releases, which must never hold game data) its result is not for distribution.
#
# The app finds the game in its own Contents/Resources/game (gamedata::find_game_dir); settings
# and saves stay in ~/Library/Application Support/OpenSMAC, so replacing the app loses nothing.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
out="$root/build/app"
name="Alpha Centauri"
apply=0
bundle_id="local.opensmac.alpha-centauri"   # marks an app as made by this script
exe_md5="cc659f53cb6fa0eaaddccd0a54176647"  # terranx.exe v2.0 (gamedata::kExeMd5)

while [ $# -gt 0 ]; do
    case "$1" in
    --apply) apply=1 ;;
    --out) out="$2"; shift ;;
    --name) name="$2"; shift ;;
    -h | --help) sed -n '2,15p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "package_app: unknown option $1 (see --help)" >&2; exit 1 ;;
    esac
    shift
done

say() { printf '  %s\n' "$1"; }
die() { printf 'package_app: %s\n' "$1" >&2; exit 1; }

build="$root/build/native"
game="$root/game"
sdl="$root/tools/sdl2"
ini="$root/harness/oracle/runtime/Alpha Centauri.Ini"
app="$out/$name.app"
libs="libSDL2-2.0.0.dylib libSDL2_ttf-2.0.0.dylib"

# Checks (both modes; nothing is written here).
[ "$(uname)" = Darwin ] || die "this script builds a macOS application"
[ -f "$game/terranx.exe" ] || die "no game files in $game (run setup.sh first)"
[ "$(md5 -q "$game/terranx.exe")" = "$exe_md5" ] || die "$game/terranx.exe is not Alien Crossfire v2.0"
for lib in $libs; do [ -f "$sdl/lib/$lib" ] || die "$lib not found in $sdl/lib (run setup.sh first)"; done
[ -f "$ini" ] || die "default ini not found: $ini"
if [ -e "$app" ] && [ "$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$app/Contents/Info.plist" 2>/dev/null)" != "$bundle_id" ]; then
    die "$app exists and was not made by this script; it is left as it is"
fi
version="$(git -C "$root" describe --tags --match 'v*' --abbrev=0 2>/dev/null | sed 's/^v//')"
version="${version:-0.0.0}"
commit="$(git -C "$root" rev-parse --short HEAD 2>/dev/null || echo unknown)"
if [ -n "$(git -C "$root" status --porcelain 2>/dev/null)" ]; then commit="$commit+changes"; fi
game_files=$(find "$game" -type f ! -path "$game/saves/*" ! -name .DS_Store | wc -l | tr -d ' ')

if [ "$apply" = 0 ]; then
    echo "package_app: dry run (nothing is written; --apply does it)"
    say "would: bring build/native/opensmac up to date (make)"
    if [ -e "$app" ]; then say "would: replace $app"; else say "would: create $app"; fi
    say "would: put in it opensmac $version ($commit), SDL2 and SDL2_ttf, $game_files game files ($(du -sh "$game" | cut -f1 | tr -d ' '))"
    say "would: sign it ad hoc, verify the signature and play five hidden turns from the bundle"
    exit 0
fi

echo "package_app: building $app"
say "compiling ..."
mkdir -p "$out"
if [ -d "$root/tools/venv/bin" ]; then PATH="$root/tools/venv/bin:$PATH"; fi   # setup.sh's cmake
(cd "$root" && make CMAKE_ARGS="-DCMAKE_PREFIX_PATH=$sdl") > "$out/build.log" 2>&1 || die "build failed: see $out/build.log"
rm -f "$out/build.log"

# Everything is assembled in a staging bundle and moved into place when it has passed.
stage="$out/.staging.app"
rm -rf "$stage"
c="$stage/Contents"
mkdir -p "$c/MacOS" "$c/Frameworks" "$c/Resources/licenses"

cp "$build/opensmac" "$c/MacOS/opensmac"
cp "$ini" "$c/Resources/"
cp "$root/LICENSE" "$c/Resources/licenses/OpenSMAC.txt"
for d in "$sdl"/licenses/*/; do
    [ -f "$d/LICENSE.txt" ] && cp "$d/LICENSE.txt" "$c/Resources/licenses/$(basename "$d").txt"
done

plist="$c/Info.plist"
sed "s/@VERSION@/$version/" "$root/packaging/Info.plist" > "$plist"
/usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier $bundle_id" -c "Set :CFBundleName $name" \
    -c "Set :CFBundleDisplayName $name" -c "Add :OpenSMACCommit string $commit" "$plist"

# The SDL libraries, found through @rpath = Contents/Frameworks only (as packaging/package.sh).
for lib in $libs; do
    cp -L "$sdl/lib/$lib" "$c/Frameworks/$lib"
    chmod u+w "$c/Frameworks/$lib"
    install_name_tool -id "@rpath/$lib" "$c/Frameworks/$lib"
done
for bin in "$c/MacOS/opensmac" "$c/Frameworks/"*.dylib; do
    otool -L "$bin" | tail -n +2 | awk '{print $1}' | { grep -E 'libSDL2(_ttf)?-2\.0\.0\.dylib$' || true; } | while read -r dep; do
        [ "$dep" = "@rpath/$(basename "$dep")" ] || install_name_tool -change "$dep" "@rpath/$(basename "$dep")" "$bin"
    done
done
otool -l "$c/MacOS/opensmac" | awk '$1 == "cmd" && $2 == "LC_RPATH" {getline; getline; print $2}' | while read -r rp; do
    install_name_tool -delete_rpath "$rp" "$c/MacOS/opensmac"
done
install_name_tool -add_rpath "@executable_path/../Frameworks" "$c/MacOS/opensmac"
strip -x "$c/MacOS/opensmac" 2>/dev/null || die "strip failed"

# The game: a copy (a clone on APFS, so it takes no time or space), without saves of test runs.
say "copying $game_files game files ..."
cp -cR "$game" "$c/Resources/game" 2>/dev/null || cp -R "$game" "$c/Resources/game"
rm -rf "$c/Resources/game/saves"
find "$c/Resources/game" \( -name .DS_Store -o -type l \) -delete
mkdir "$c/Resources/game/saves"

say "signing ..."
codesign --force --sign - "$c/Frameworks/"*.dylib 2>/dev/null || die "signing the libraries failed"
codesign --force --sign - "$stage" 2>/dev/null || die "signing the app failed"
codesign --verify --deep --strict "$stage" || die "the signature does not verify; $app is unchanged"
if otool -L "$c/MacOS/opensmac" "$c/Frameworks/"*.dylib | grep -E "^[[:space:]]" | grep -vE "@rpath/|/usr/lib/|/System/Library/" | grep -q .; then
    die "the app still refers to libraries outside itself; $app is unchanged"
fi

# Five hidden turns played by the bundle's own binary, libraries and game files.
say "playing five hidden turns from the bundle ..."
smoke="$out/.smoke"
rm -rf "$smoke"
mkdir -p "$smoke"
python3 "$root/src/tools/gen_data_image.py" "$c/Resources/game/terranx.exe" "$smoke/data_image.bin" >/dev/null
(cd "$smoke" && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$c/MacOS/opensmac" --new size=0 --human 0 --seed 1,2 \
    --hidden --turns 5 --shot "$smoke/shot.bmp" --outdir "$smoke/out" --gamedir "$c/Resources/game" \
    --image "$smoke/data_image.bin" --ini "$c/Resources/Alpha Centauri.Ini") > "$smoke/log.txt" 2>&1 ||
    die "the bundled game did not run: see $smoke/log.txt; $app is unchanged"
[ -s "$smoke/shot.bmp" ] || die "the bundled game drew nothing: see $smoke/log.txt; $app is unchanged"
rm -rf "$smoke"
# The run above saved into the bundle's saves folder: empty it and sign once more.
rm -rf "$c/Resources/game/saves"
mkdir "$c/Resources/game/saves"
codesign --force --sign - "$stage" 2>/dev/null || die "signing the app failed"
codesign --verify --deep --strict "$stage" || die "the signature does not verify; $app is unchanged"

rm -rf "$app"   # checked above: absent, or an app this script made
mv "$stage" "$app"
echo "package_app: done: $app ($(du -sh "$app" | cut -f1 | tr -d ' '), opensmac $version $commit)"
echo "  Open it from Finder, or: open \"$app\""
