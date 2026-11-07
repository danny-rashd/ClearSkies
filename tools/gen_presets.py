"""Generate src/cemu_presets.h from Cemu's Clarity graphic pack (third_party/cemu, CC0 1.0).

Reads every Clarity preset block (#if / #elif (Preset == N)) and the shader's functions out of
37040a485a29d54e_00000000000003c9_ps.txt, and the preset names out of rules.txt, so all of Cemu's presets come
across exactly as they are in the pack. Run it again after updating the files in third_party/cemu:

    py -I tools/gen_presets.py
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SHADER = ROOT / "third_party" / "cemu" / "37040a485a29d54e_00000000000003c9_ps.txt"
RULES = ROOT / "third_party" / "cemu" / "rules.txt"
OUT = ROOT / "src" / "cemu_presets.h"

# Cemu preset number -> the name used in clearskies.ini (and on the launcher's command line)
IDS = {
    0: "custom", 1: "bruz", 2: "t3nka", 3: "jamie", 4: "contrasty", 5: "complaining_gamer", 6: "filmic",
    7: "cellhunter", 8: "serfrost_bright", 9: "serfrost_dim", 10: "serfrost", 11: "viskclarity",
    12: "sotw_warmer", 13: "sotw_colder", 14: "godot_mexican", 15: "hexaae_clear", 16: "maranbr",
    17: "yemaowuxin_bright", 18: "yemaowuxin_vivid", 19: "cobalt",
}
# The order the launcher lists them in: Cemu's own order in rules.txt
DEFINE = re.compile(r"^#define\s+(\w+)\s+(-?\d+)\s*$")
SCALAR = re.compile(r"^(?:const\s+)?(float|int)\s+(\w+)\s*=\s*(-?[\d.]+)\s*;$")
VEC3 = re.compile(r"^(?:const\s+)?vec3\s+(\w+)\s*=\s*vec3\(([^)]*)\)\s*;$")


def die(msg):
    sys.exit("gen_presets: " + msg)


def parse_blocks(lines):
    blocks, cur = {}, None
    for line in lines:
        s = line.split("//")[0].strip()
        m = re.match(r"^#(?:el)?if\s*\(Preset == (\d+)\)", s)
        if m:
            cur = int(m.group(1))
            blocks[cur] = []
            continue
        if cur is None:
            continue
        if s == "#endif":
            break
        if not s:
            continue
        if m := DEFINE.match(s):
            blocks[cur].append((m.group(1), "d", [float(m.group(2))]))
        elif m := SCALAR.match(s):
            blocks[cur].append((m.group(2), "f" if m.group(1) == "float" else "i", [float(m.group(3))]))
        elif m := VEC3.match(s):
            v = [float(x) for x in m.group(2).split(",")]
            if len(v) != 3:
                die(f"preset {cur}: {s}")
            blocks[cur].append((m.group(1), "v", v))
        else:
            die(f"preset {cur}: can't read '{s}'")
    return blocks


def parse_names():
    names, name = {}, None
    for line in RULES.read_text(encoding="utf-8").splitlines():
        s = line.strip()
        if s.startswith("name ="):
            name = s.split("=", 1)[1].strip()
        m = re.match(r"^\$preset:int\s*=\s*(\d+)", s)
        if m and name:
            names[int(m.group(1))] = name
    order = [n for n in names]  # rules.txt order
    return names, order


def functions(lines):
    """The shader's functions: from //ToneMapping to just before main(), adapted to Savage Wind."""
    text = "\n".join(lines)
    a, b = text.index("//ToneMapping"), text.index("void main()")
    body = text[a:b]
    # One texel of the HDR scene (Cemu: 1/1280 or 1/720 times its resolution scale, the same thing)
    body = body.replace("#define px (1.0/1280.0*uf_fragCoordScale.x)", "#define px (1.0 / float(textureSize(t0, 0).x))")
    body = body.replace("#define py (1.0/720.0*uf_fragCoordScale.y)", "#define py (1.0 / float(textureSize(t0, 0).y))")
    if "uf_fragCoordScale" in body:
        die("uf_fragCoordScale is still used in the functions")
    # The Switch shader clamps the HDR scene at 10000 before use (an infinite texel would turn into NaN)
    n = body.count("texture(tex, ")
    body = body.replace("texture(tex, ", "hdr_tex(tex, ")
    if n < 20:
        die(f"expected the HDR / sharpening taps, found {n}")
    return body


def c_float(x):
    s = f"{x:.7g}"
    if "." not in s and "e" not in s:
        s += ".0"
    return s + "f"


def c_string(text):
    out = []
    for line in text.splitlines():
        line = line.replace("\\", "\\\\").replace('"', '\\"').replace("\t", "    ").rstrip()
        out.append(f'    "{line}\\n"')
    return "\n".join(out)


def main():
    lines = SHADER.read_text(encoding="utf-8").splitlines()
    blocks = parse_blocks(lines)
    names, order = parse_names()
    if sorted(blocks) != list(range(20)):
        die(f"expected presets 0-19, found {sorted(blocks)}")
    if set(names) != set(blocks):
        die("rules.txt and the shader list different presets")

    params = [(n, k) for n, k, _ in blocks[0]]
    for p, b in blocks.items():
        if [(n, k) for n, k, _ in b] != params:
            # Same settings, maybe in another order or declared const / not const: compare by name.
            have = {n: k for n, k, _ in b}
            want = dict(params)
            if have.keys() != want.keys():
                die(f"preset {p} has different settings: {sorted(set(have) ^ set(want))}")
            for n in want:
                if want[n] != have[n] and {want[n], have[n]} != {"f", "i"}:
                    die(f"preset {p}: {n} is {have[n]}, preset 0 has {want[n]}")

    o = []
    o.append("// Generated by tools/gen_presets.py from Cemu's Clarity graphic pack (third_party/cemu). Do not edit.")
    o.append("// Clarity GFX by Jamie and Kiri with Crementif; presets by their named creators. CC0 1.0.")
    o.append("#pragma once\n")
    o.append("// kind: 'd' a #define switch (whole number), 'f' float, 'i' int, 'v' vec3")
    o.append("typedef struct { const char *name; char kind; } CemuParam;")
    o.append("static const CemuParam k_params[] = {")
    for n, k in params:
        o.append(f'    {{"{n}", \'{k}\'}},')
    o.append("};")
    o.append("#define CLARITY_NPARAMS ((int)(sizeof k_params / sizeof k_params[0]))\n")
    o.append("typedef struct { const char *id, *title; int cemu_number; float v[%d][3]; } CemuPreset;" % len(params))
    o.append("// In the order of Cemu's menu (rules.txt)")
    o.append("static const CemuPreset k_presets[] = {")
    for p in order:
        vals = {n: v for n, _, v in blocks[p]}
        row = ", ".join("{" + ", ".join(c_float(x) for x in (vals[n] + [0, 0])[:3]) + "}" for n, _ in params)
        title = names[p].replace('"', '\\"')
        o.append(f'    {{"{IDS[p]}", "{title}", {p}, {{{row}}}}},')
    o.append("};")
    o.append("#define CLARITY_NPRESETS ((int)(sizeof k_presets / sizeof k_presets[0]))\n")
    o.append("// Cemu's shader functions (tone maps, passes), adapted: px / py from the scene texture, HDR taps clamped.")
    o.append("static const char k_cemu_functions[] =")
    o.append(c_string(functions(lines)) + ";")
    OUT.write_text("\n".join(o) + "\n", encoding="utf-8", newline="\n")
    print(f"gen_presets: {len(order)} presets, {len(params)} settings -> {OUT.relative_to(ROOT)}")
    write_ini(blocks, names, order, params)


def write_ini(blocks, names, order, params):
    """The clearskies.ini that ships: the preset, every preset's name, and every setting (commented out)."""
    kinds = {"d": "switch", "f": "number", "i": "whole number", "v": "colour: three numbers"}
    o = [
        "; Clear Skies: a mod for Savage Wind that adds Cemu's Clarity presets.",
        "; The launcher changes 'preset' for you. Changes here take effect the next time the game starts.",
        "",
        "[clarity]",
        "preset = serfrost",
        "",
        "; Presets (Cemu's Clarity graphic pack, CC0 1.0; each by its named creator):",
        ";   off                  the game's own look",
    ]
    for p in order:
        o.append(f";   {IDS[p]:<20} {names[p]}")
    o += [
        "",
        "; Any setting below changes the chosen preset. Remove the ; in front of a line to use it.",
        "; The values shown are Serfrost's preset. Switches are 1 (on) or 0 (off).",
        "; Tone_map: -1 none (clamp), 0 BotW, 1 Reshade, 2 linear, 3 Reinhard, 4 luma Reinhard,",
        ";           5 white-preserving Reinhard, 6 RomBinDaHouse, 7 filmic, 8 Uncharted 2, 9 ACES",
    ]
    serf = {n: v for n, _, v in blocks[10]}
    for n, k in params:
        v = serf[n]
        val = ", ".join(f"{x:g}" for x in v) if k == "v" else f"{v[0]:g}"
        o.append(f"; {n} = {val}" + " " * max(1, 34 - len(n) - len(val)) + f"; {kinds[k]}")
    o += ["", "[launcher]", "game = ", ""]
    path = ROOT / "clearskies.ini"
    path.write_text("\n".join(o), encoding="utf-8", newline="\r\n")
    print(f"gen_presets: -> {path.relative_to(ROOT)}")


main()
