<p align="center"><img src="docs/art/banner.png" alt="Clear Skies"></p>

**Clear Skies** is a mod for [Savage Wind Recomp](https://github.com/Revivedx/SavageWindRecomp), the native PC version of *The Legend of Zelda: Breath of the Wild*. It brings over **Clarity**, the much-loved look from Cemu's Breath of the Wild graphic pack, with **all 20 of its presets**. You get less haze, less blown-out bloom and richer colour, without changing a single file of Savage Wind.

**[Download the newest release](../../releases)** · [Install it](#how-to-install) · [The presets](#the-presets) · [Help test it](#help-test-it)

> [!CAUTION]
> **No game files are included, and none ever will be**: not in the code, the releases, the issues or the wiki. Clear Skies doesn't include Savage Wind either. You need your own working copy of Savage Wind, set up with your own game files, dumped from your own Switch. Please don't post game files, dumps, keys or an installed copy here.

> [!IMPORTANT]
> **Made with AI.** Every line of code in Clear Skies, every test, the icon, the banner and this page were written or made by Claude, Anthropic's AI, through Claude Code. A human, [devall96](https://github.com/devall96), came up with the idea, directs it and plays every build, but no human reviews the code line by line. [Who did what](#who-did-what) has the details. If you don't want to run code written that way, don't run this. As with anything on the internet, you run it at your own risk.

> [!WARNING]
> **Early, and tested on one PC.** Clear Skies has run in the game on one Windows PC with an AMD Radeon RX 9070 XT, using Serfrost's preset with HDR off. Every preset is tested automatically on that GPU, but NVIDIA, Intel, HDR output, Linux and Steam Deck haven't been tried yet. Clear Skies doesn't touch your saves, but it's still worth keeping a copy of any you care about.

> [!NOTE]
> **Not official.** Clear Skies is a fan-made mod. It isn't made by, affiliated with or endorsed by the Savage Wind Recomp team, the Cemu team or Nintendo. Please don't report Clear Skies problems to Savage Wind or Cemu: [open an issue here](../../issues) instead.

## Why this exists

Clarity was one of the best parts of my own Savage Wind fork, [Slop of the Wild](https://github.com/devall96/Slop-of-the-Wild). But you shouldn't have to switch to a whole fork just to get one look. Clear Skies puts Clarity on plain Savage Wind as a separate, add-on mod: it keeps up with Savage Wind's own updates, it can be turned off by starting the game normally, and it brings every Cemu preset along, not just the three Slop of the Wild had.

## Screenshots

<p align="center"><img src="docs/launcher.png" alt="The Clear Skies launcher: choose a look, point it at savagewind.exe, press Play" width="576"></p>

In-game comparisons are coming. If you take good before-and-after shots, [share them in an issue](../../issues).

## What it adds

**The look**
- Cemu's **Clarity** for Breath of the Wild in place of the game's own tone mapping: Clarity GFX by Jamie and Kiri with Crementif
- **All 20 presets** from Cemu's graphic pack, from Serfrost's recommended preset to Filmic, Survival of the Wild and Hexaae's Clear
- **All 65 of Clarity's settings**, with the same names as in Cemu's pack, adjustable on top of any preset: bloom, fake HDR, sharpening, ten tone maps, levels, Technicolor, colour matrix, filmic pass, DPX, curves, lift / gamma / gain and vibrance

**The launcher**
- A small themed window: choose a look, point it at `savagewind.exe`, press **Play**
- Remembers your choice, warns you if Savage Wind isn't set up yet, and has command-line options for shortcuts

**How it stays out of the way**
- **Nothing in Savage Wind's folder is changed.** Clear Skies runs as a Vulkan layer, switched on only for the game it starts. Start `savagewind.exe` directly and you get the original look.
- **Nothing is installed system-wide.** Delete the folder and it's gone.
- **Your saves aren't touched.** The layer only replaces one shader on its way to your graphics card.
- **Works with Savage Wind's updates**, as long as the game keeps building that shader the same way (the log tells you if it doesn't)

## How to install

1. Have a working **[Savage Wind](https://github.com/Revivedx/SavageWindRecomp)** first: set up with your game files, and started at least once. For the smoothest first run, also use **Precompile shaders** in Savage Wind Configuration.
2. **[Download the newest Clear Skies](../../releases)** and unzip the `ClearSkies` folder anywhere. Its files must stay together.
3. Run **ClearSkies.exe**, choose a look, click **Browse** to find your `savagewind.exe`, and press **Play**.

From then on, start the game with ClearSkies.exe whenever you want Clarity. To change the look, pick another preset and press Play again: the game needs a restart to switch.

**Shortcuts and command line**

| | |
| --- | --- |
| `ClearSkies.exe --play` | Start straight away with the saved preset |
| `ClearSkies.exe --preset filmic --play` | Choose a preset (it's saved) and start |
| `ClearSkies.exe --game "D:\Savage Wind\savagewind.exe"` | Choose the game |
| `ClearSkies.exe -- --config` | Anything after `--` goes to Savage Wind |

**Uninstalling:** delete the `ClearSkies` folder. Nothing else was changed.

## The presets

| `preset =` | From Cemu's pack |
| --- | --- |
| `serfrost` | Serfrost's Preset (Recommended), the default |
| `serfrost_dim` | Serfrost's Preset (Dim Display) |
| `serfrost_bright` | Serfrost's Preset (Bright Display) |
| `bruz` | Bruz's Preset |
| `cellhunter` | CellHunter's Preset |
| `jamie` | Jamie's Preset |
| `t3nka` | t3nka Neutral+ |
| `complaining_gamer` | The Complaining Gamer's Preset |
| `viskclarity` | ViskClarity Preset |
| `contrasty` | Contrasty Preset |
| `filmic` | Filmic Preset |
| `sotw_warmer` | Silentverge's Survival of the Wild Preset (Warmer) |
| `sotw_colder` | Silentverge's Survival of the Wild Preset (Colder) |
| `godot_mexican` | Godot's Mexican Preset |
| `hexaae_clear` | Hexaae's Clear Preset |
| `maranbr` | MaranBr Preset (Little More Intense) |
| `yemaowuxin_bright` | YeMaoWuXin's Preset (Bright colors) |
| `yemaowuxin_vivid` | YeMaoWuXin's Preset (Vivid and clear) |
| `cobalt` | Cobalt's Preset (Saturated & Bright) |
| `custom` | User-Defined Preset: Cemu's starting values, for building your own |
| `off` | The game's own look |

### Making your own

`clearskies.ini` (the launcher's **Edit settings** button) lists every one of Clarity's settings, using the names from Cemu's pack. Remove the `;` in front of a setting to change it on top of the chosen preset:

```ini
[clarity]
preset = serfrost
bloomFactor = 0.2
VibranceRGBBalance = 1.0, 1.0, 1.4
Tone_map = 9
```

Switches are `1` (on) or `0` (off), and colours are three numbers. `Tone_map` picks the tone map: -1 none, 0 BotW, 1 ReShade, 2 linear, 3 Reinhard, 4 luma Reinhard, 5 white-preserving Reinhard, 6 RomBinDaHouse, 7 filmic, 8 Uncharted 2, 9 ACES. Changes apply the next time the game starts. If a value can't be used, the log says so and that one setting keeps the preset's value.

## How it works

Breath of the Wild finishes every frame with one shader. It adds the bloom to the HDR image and tone-maps the result for your screen. On the Wii U, Cemu's Clarity pack replaces that shader. Clear Skies replaces the same shader in Savage Wind:

1. **ClearSkies.exe** starts `savagewind.exe` with the Vulkan loader told to load the Clear Skies layer, for that one process only.
2. **clearskies_layer.dll** sits between the game and the graphics driver and looks at every shader the game creates. It recognises the tone-mapping shader by what's inside it: its two textures, and the constants BotW's tone map uses (the 10000 clamp, the 0.2989 luma weight and −log₂ e).
3. When it finds it, the layer builds Clarity with your preset's settings, using the shader compiler that ships with Savage Wind, and hands that to the driver instead. Every other shader passes through untouched.

The Clarity code itself comes from Cemu's pack: `tools/gen_presets.py` reads the presets and the shader's functions straight out of Cemu's files (kept unchanged in `third_party/cemu`), so nothing is retyped by hand. The only changes are what Savage Wind needs: the shader's inputs, the pixel size, and clamps that match the game's own shader ([LICENSES.txt](LICENSES.txt) lists them).

## Troubleshooting

Every run writes **`clearskies.log`** next to `clearskies.ini`. A working run looks like this:

```
Clear Skies loaded into C:\...\savagewind.exe
preset: serfrost (Cemu's "Serfrost's Preset (Recommended)")
watching shaders on a new Vulkan device
Clarity compiled (32140 bytes of SPIR-V)
Clarity replaces the tone-mapping composite (shader 1)
```

| What you see | What it means |
| --- | --- |
| Savage Wind opens its setup screen | That copy of Savage Wind isn't set up yet. Finish its setup, then use Clear Skies. |
| No log, or no `loaded into` line | The layer didn't load. Start the game from ClearSkies.exe and keep all of Clear Skies' files together. |
| `failed to compile` | A value in `clearskies.ini` can't be used. The message says which. Remove that line. |
| Loaded, but never `replaces the tone-mapping composite` | Your Savage Wind builds that shader differently. Please [report it](../../issues) with the log. |
| Stutters the first time you see places | That's Savage Wind compiling shaders. Run **Precompile shaders** in Savage Wind Configuration. |

## Help test it

So far Clear Skies has been tested on one PC, so **reports from other hardware are the most useful thing you can send.** That goes for NVIDIA and Intel cards, older AMD cards, HDR monitors, laptops, Linux and Steam Deck. Good news helps too: "it works on this card" tells me what works.

To report, [open an issue](../../issues) with:
- Your graphics card and driver version
- Which preset, and whether HDR is on in Savage Wind
- `clearskies.log` from the Clear Skies folder, and `savagewind.log` from Savage Wind's folder
- A screenshot if something looks wrong

**Never attach game files, dumps, keys or saves.**

## Build it yourself

On Windows you need [Zig](https://ziglang.org/download/) (a complete C compiler in one download) and [Python 3](https://www.python.org/downloads/):

```bash
git clone https://github.com/devall96/ClearSkies
```

```bash
powershell -ExecutionPolicy Bypass -File build.ps1 -Zig C:\path\to\zig.exe
```

This writes the mod to `dist\ClearSkies`. To test the layer on your own GPU against every preset, point the test at a Savage Wind folder (it uses that folder's `libshaderc_shared.dll`):

```bash
powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1 -SavageWind "C:\Games\Savage Wind"
```

| | |
| --- | --- |
| `src/clearskies_layer.c` | The Vulkan layer: finds the tone-mapping shader and swaps in Clarity |
| `src/clarity_shader.h` | Clarity's inputs and `main()` for Savage Wind |
| `src/cemu_presets.h` | Generated: every preset and Clarity's functions, from Cemu's files |
| `src/launcher.c` | ClearSkies.exe |
| `tools/gen_presets.py` | Generates `cemu_presets.h` and `clearskies.ini` from `third_party/cemu` |
| `tools/make_icon.py`, `tools/make_banner.ps1` | Draw the icon and this page's banner |
| `tests/` | The layer test, run on a real GPU |

## Who did what

**devall96, the human:** the idea, the name, what it should and shouldn't do, testing in the game, and deciding what stays.

**Claude (Anthropic's AI, Claude Opus 5.5 through Claude Code):** all of the code, the tests, the preset generator, the icon and banner (drawn by scripts), the license research and this page. That includes porting Cemu's Clarity shader to Savage Wind and working out how to recognise the game's tone-mapping shader. No human has reviewed the code line by line.

**Cemu's graphic pack authors:** Clarity itself and every preset. **Savage Wind and its contributors:** everything Clear Skies builds on.

## Credits

- **Clarity** from the [Cemu graphic packs](https://github.com/cemu-project/cemu_graphic_packs) (CC0 1.0): Clarity GFX by Jamie (main code) and Kiri (code and ReShade logic), with Crementif. Presets by Serfrost, Bruz, CellHunter, Jamie, t3nka, The Complaining Gamer, Visk, Silentverge, Godot, Hexaae, MaranBr, YeMaoWuXin and Cobalt. Contrasty, Filmic and the User-Defined starting values come with the pack without a named creator. Several passes are ports of ReShade effects, as they appear in the pack.
- **[Savage Wind Recomp](https://github.com/Revivedx/SavageWindRecomp)** by Revivedx and contributors, built on **[LAS Recomp](https://github.com/peiikai/LASRecomp)** by Peiikai (MIT). Not included: Clear Skies works with your own copy.
- The banner's lettering is [Fira Sans](https://github.com/mozilla/Fira) by the Mozilla Foundation and Telefónica (SIL Open Font License 1.1, `third_party/fonts/FiraSans`).
- The few Vulkan declarations the layer needs follow the Khronos [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers) (Apache-2.0 or MIT). Vulkan is a registered trademark of the Khronos Group Inc.
- *The Legend of Zelda: Breath of the Wild* is © Nintendo. *Nintendo Switch* and *The Legend of Zelda* are trademarks of Nintendo. This project is not affiliated with or endorsed by Nintendo.

## License

Clear Skies' own code is under the [MIT License](LICENSE). Material from others keeps its own license: Cemu's Clarity pack is CC0 1.0 and Fira Sans is OFL 1.1. [LICENSES.txt](LICENSES.txt) has the full list, including exactly what was adapted.
