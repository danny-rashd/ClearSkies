#!/bin/sh
# Clear Skies for Linux: starts Savage Wind with the Clear Skies layer turned on, for that one process only.
#
#   ./clearskies.sh                          start the game with the saved preset (--play does the same)
#   ./clearskies.sh --preset filmic          choose a preset (it's saved) and start
#   ./clearskies.sh --game ~/Games/Savage_Wind-x86_64.AppImage
#                                            choose the game (it's saved) and start
#   ./clearskies.sh --list                   list the presets
#   ./clearskies.sh --edit                   open clearskies.ini in your editor
#   ./clearskies.sh --no-play ...            only save the choices, don't start
#   ./clearskies.sh ... -- --config          anything after -- goes to Savage Wind
#   ./clearskies.sh %command%                (Steam launch options) wrap the command Steam gives
#
# Nothing is installed system-wide: the Vulkan loader finds the layer through environment variables set here.
set -eu

LAYER_NAME=VK_LAYER_CLEARSKIES_savage_wind
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)
INI=$HERE/clearskies.ini
TITLE="Clear Skies"

say() { printf '%s: %s\n' "$TITLE" "$*" >&2; }
die() { say "$*"; exit 1; }

# ---- clearskies.ini: read and write one key in one section (keeps the rest, and the file's CRLF line endings)

ini_get() {  # section key
    [ -f "$INI" ] || return 0
    awk -v sec="$1" -v key="$2" '
        { sub(/\r$/, "") }
        /^[ \t]*\[/ { s = $0; gsub(/^[ \t]*\[|\][ \t]*$/, "", s); cur = s; next }
        cur == sec {
            line = $0; sub(/[;#].*/, "", line)
            i = index(line, "="); if (!i) next
            k = substr(line, 1, i - 1); v = substr(line, i + 1)
            gsub(/^[ \t]+|[ \t]+$/, "", k); gsub(/^[ \t]+|[ \t]+$/, "", v)
            if (tolower(k) == tolower(key)) { print v; exit }
        }' "$INI"
}

ini_set() {  # section key value
    tmp=$INI.tmp.$$
    if [ -f "$INI" ]; then src=$INI; else src=/dev/null; fi
    awk -v sec="$1" -v key="$2" -v val="$3" '
        function emit(s) { printf "%s\r\n", s }
        { sub(/\r$/, "") }
        /^[ \t]*\[/ {
            if (cur == sec && !done) { emit(key " = " val); done = 1 }
            s = $0; gsub(/^[ \t]*\[|\][ \t]*$/, "", s); cur = s; emit($0); next
        }
        cur == sec && !done {
            line = $0; i = index(line, "=")
            if (i && line !~ /^[ \t]*[;#]/) {
                k = substr(line, 1, i - 1); gsub(/^[ \t]+|[ \t]+$/, "", k)
                if (tolower(k) == tolower(key)) { emit(key " = " val); done = 1; next }
            }
        }
        { emit($0) }
        END {
            if (!done) { if (cur != sec) { emit(""); emit("[" sec "]") } emit(key " = " val) }
        }' "$src" >"$tmp" && mv -f "$tmp" "$INI"
}

# ---- presets: the list in clearskies.ini's comments ("  ;   name   Title")

list_presets() {
    awk '{ sub(/\r$/, "") } /^;   [a-z0-9_]+  +/ { print substr($0, 5) }' "$INI"
}

valid_preset() {
    case $(printf '%s' "$1" | tr '[:upper:]' '[:lower:]') in
        off|disabled|none|recommended|dim|bright|user|user_defined) return 0 ;;
        [0-9]|[0-9][0-9]) return 0 ;;  # (Cemu's preset numbers)
    esac
    list_presets | awk -v p="$1" 'tolower($1) == tolower(p) { found = 1 } END { exit !found }'
}

# ---- arguments

preset='' game='' play=1 edit=0 wrapped=0
while [ $# -gt 0 ]; do
    case $1 in
        --preset) [ $# -ge 2 ] || die "--preset needs a name (--list shows them)"; preset=$2; shift 2 ;;
        --preset=*) preset=${1#*=}; shift ;;
        --game) [ $# -ge 2 ] || die "--game needs the path to Savage Wind"; game=$2; shift 2 ;;
        --game=*) game=${1#*=}; shift ;;
        --play) play=1; shift ;;
        --no-play) play=0; shift ;;
        --list) list_presets; exit 0 ;;
        --edit) edit=1; play=0; shift ;;
        -h|--help) sed -n '2,14s/^# \{0,1\}//p' "$0"; exit 0 ;;
        --) shift; break ;;
        -*) die "unknown option $1 (--help lists them)" ;;
        *) wrapped=1; break ;;  # (Steam's %command%: the game and its arguments)
    esac
done
[ -f "$INI" ] || die "clearskies.ini is missing from $HERE. Keep all of Clear Skies' files together."

if [ -n "$preset" ]; then
    valid_preset "$preset" || die "unknown preset '$preset' (--list shows them)"
    ini_set clarity preset "$preset"
fi
if [ -n "$game" ]; then
    case $game in /*) ;; *) game=$PWD/$game ;; esac
    [ -f "$game" ] || die "no such file: $game"
    ini_set launcher game "$game"
fi

if [ "$edit" = 1 ]; then
    if [ -n "${VISUAL:-${EDITOR:-}}" ]; then exec ${VISUAL:-$EDITOR} "$INI"; fi
    command -v xdg-open >/dev/null 2>&1 && exec xdg-open "$INI"
    die "set EDITOR, or open $INI yourself"
fi
[ "$play" = 1 ] || { say "preset: $(ini_get clarity preset)"; exit 0; }

# ---- checks

if [ ! -f "$HERE/libclearskies_layer.so" ] || [ ! -f "$HERE/VkLayer_clearskies.json" ]; then
    die "libclearskies_layer.so or VkLayer_clearskies.json is missing from $HERE. Keep all of Clear Skies' files together."
fi

if [ "$wrapped" = 0 ]; then
    game=$(ini_get launcher game)
    [ -n "$game" ] || die "choose the game first: ./clearskies.sh --game /path/to/Savage_Wind-x86_64.AppImage"
    [ -f "$game" ] || die "the saved game isn't there any more: $game (choose it again with --game)"
    [ -x "$game" ] || die "$game isn't executable: chmod +x it first"
fi

# Savage Wind opens its own setup screen until the game files are copied and the game code is built.
data=${SAVAGEWIND_DATA_ROOT:-${XDG_DATA_HOME:-$HOME/.local/share}/savagewind}
if [ ! -d "$data/extracted/romfs" ]; then
    say "Savage Wind hasn't finished setting up its game files in $data yet, so it will open its own setup screen first."
    say "Clarity applies once the game itself is running."
fi

# ---- start

prepend() {  # variable value: value:old (unless it's already there)
    old=''
    eval "old=\${$1:-}"
    case ":$old:" in *":$2:"*) ;; *) eval "export $1=\"\$2\${old:+:\$old}\"" ;; esac
}
prepend VK_ADD_LAYER_PATH "$HERE"  # (Vulkan loader 1.3.234 and newer)
prepend VK_LAYER_PATH "$HERE"      # (older loaders)
prepend VK_INSTANCE_LAYERS "$LAYER_NAME"
export CLEARSKIES_INI="$INI"

say "starting with preset $(ini_get clarity preset); the log is $HERE/clearskies.log"
if [ "$wrapped" = 1 ]; then exec "$@"; fi
cd "$(dirname -- "$game")"
exec "$game" "$@"
