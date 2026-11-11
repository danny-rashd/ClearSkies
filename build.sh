#!/bin/sh
# Clear Skies for Linux: build with the system's C compiler, or with Zig (zig cc) from any OS.
#   ./build.sh                     cc (gcc or clang) for this machine
#   CC="zig cc -target x86_64-linux-gnu.2.35" ./build.sh
#                                  cross-compile, e.g. from macOS (glibc 2.35: the oldest Savage Wind supports)
# Output: dist/ClearSkies-linux/ (the mod, ready to tar), build/layer_test (tests/run_tests.sh)
set -eu
ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)
CC=${CC:-cc}
OUT=$ROOT/dist/ClearSkies-linux
CFLAGS="-O2 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers"
mkdir -p "$ROOT/build" "$OUT"

if command -v python3 >/dev/null 2>&1; then python3 -I "$ROOT/tools/gen_presets.py"; fi

# -fvisibility=hidden + -Bsymbolic: only the loader's three entry points are exported, and the layer's own calls never
# bind to the Vulkan loader's functions of the same name in the game's process ($CC and $CFLAGS split into words)
# shellcheck disable=SC2086
$CC $CFLAGS -fPIC -shared -fvisibility=hidden -Wl,-Bsymbolic -Wl,-soname,libclearskies_layer.so \
    -o "$OUT/libclearskies_layer.so" "$ROOT/src/clearskies_layer.c" -ldl -lpthread -s
# shellcheck disable=SC2086
$CC $CFLAGS -o "$ROOT/build/layer_test" "$ROOT/tests/layer_test.c" -ldl

sed 's|"\.\\\\clearskies_layer\.dll"|"./libclearskies_layer.so"|' "$ROOT/VkLayer_clearskies.json" >"$OUT/VkLayer_clearskies.json"
grep -q '"./libclearskies_layer.so"' "$OUT/VkLayer_clearskies.json" || { echo "build: manifest not rewritten" >&2; exit 1; }
cp "$ROOT/src/clearskies.sh" "$OUT/clearskies.sh" && chmod +x "$OUT/clearskies.sh"
for f in LICENSE LICENSES.txt README.md; do cp "$ROOT/$f" "$OUT/$f"; done
[ -f "$OUT/clearskies.ini" ] || cp "$ROOT/clearskies.ini" "$OUT/clearskies.ini"  # (keeps your own settings)
ls -l "$OUT"
