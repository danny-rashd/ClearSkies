#!/bin/sh
# Clear Skies for Linux: runs build/layer_test once per preset with the layer on, and checks the layer's log.
#   tests/run_tests.sh [folder with libshaderc_shared.so.1]
# Without a folder the system's shaderc is used (Debian/Ubuntu libshaderc1, Fedora libshaderc, Arch shaderc). To use
# the one Savage Wind ships: ./Savage_Wind-*.AppImage --appimage-extract, then pass squashfs-root/usr/lib.
set -eu
ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd -P)
DIST=$ROOT/dist/ClearSkies-linux
TEST=$ROOT/build/layer_test
WORK=$ROOT/build/test
mkdir -p "$WORK"
if [ ! -x "$TEST" ] || [ ! -f "$DIST/libclearskies_layer.so" ]; then echo "run ./build.sh first"; exit 1; fi

presets="off $(awk '{ sub(/\r$/, "") } /^;   [a-z0-9_]+  +/ && $2 != "off" { print $2 }' "$DIST/clearskies.ini")"
export VK_ADD_LAYER_PATH="$DIST" VK_LAYER_PATH="$DIST" VK_INSTANCE_LAYERS=VK_LAYER_CLEARSKIES_savage_wind
export CLEARSKIES_INI="$WORK/clearskies.ini"
failed=0 count=0
for p in $presets; do
    count=$((count + 1))
    printf '[clarity]\r\npreset = %s\r\n' "$p" >"$CLEARSKIES_INI"
    rm -f "$WORK/clearskies.log"
    if out=$("$TEST" "$@" 2>&1); then code=0; else code=$?; fi
    log=$(cat "$WORK/clearskies.log" 2>/dev/null || true)
    replaced=$(printf '%s\n' "$log" | grep -c 'replaces the tone-mapping composite' || true)
    ok=0
    if [ "$code" = 0 ] && printf '%s' "$log" | grep -q 'loaded into'; then
        if [ "$p" = off ]; then [ "$replaced" = 0 ] && ok=1
        else [ "$replaced" = 1 ] && printf '%s' "$log" | grep -q 'compiled' && ok=1; fi
    fi
    if [ "$ok" = 1 ]; then printf '%-20s ok\n' "$p"
    else failed=$((failed + 1)); printf '%-20s FAIL\n%s\n%s\n' "$p" "$out" "$log"; fi
done
if [ "$failed" -gt 0 ]; then echo "$failed preset(s) failed"; exit 1; fi
echo "all $count presets ok"
