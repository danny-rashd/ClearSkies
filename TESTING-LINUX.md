# Testing Clear Skies on Linux

How to build Clear Skies on your own Linux machine and try it with Savage Wind. The commands are written for
**Arch Linux**; other distributions only differ in step 1.

> [!NOTE]
> Clear Skies for Linux has passed its automated test (all 21 presets), but it hasn't been tried in the real game on
> Linux yet. Your test is the first one, so please send back the log (step 5).

## Before you start

- **Savage Wind** as its Linux AppImage (`Savage_Wind-…-x86_64.AppImage`), set up with your own game files and
  started at least once.
- A working Vulkan driver. If Savage Wind runs, you already have one (`vulkan-icd-loader` plus `vulkan-radeon`,
  `vulkan-intel` or `nvidia-utils`).

In the commands below, replace `~/Games/Savage_Wind-0.5.1-alpha.5-x86_64.AppImage` with the path to your AppImage.

## 1. Install the tools

```bash
sudo pacman -S --needed git gcc python
```

| Distribution | Command |
| --- | --- |
| Arch | `sudo pacman -S --needed git gcc python` |
| Debian / Ubuntu | `sudo apt install git gcc python3` |
| Fedora | `sudo dnf install git gcc python3` |

If the AppImage won't start with a FUSE error:

```bash
sudo pacman -S --needed fuse3
```

## 2. Get and build Clear Skies

```bash
git clone https://github.com/danny-rashd/ClearSkies.git
```

```bash
cd ClearSkies
```

```bash
./build.sh
```

The build ends by listing `dist/ClearSkies-linux/`, which should contain:

- `libclearskies_layer.so`: the layer
- `clearskies.sh`: the launcher
- `clearskies.ini`: your settings
- `VkLayer_clearskies.json`: tells Vulkan where the layer is

## 3. Test the layer without the game (optional)

This takes about 10 seconds and checks every preset against your GPU. Choose one of the two ways to get shaderc.

**A. Use the shaderc inside your AppImage:**

```bash
~/Games/Savage_Wind-0.5.1-alpha.5-x86_64.AppImage --appimage-extract >/dev/null
```

```bash
tests/run_tests.sh squashfs-root/usr/lib
```

```bash
rm -rf squashfs-root
```

**B. Or use Arch's shaderc package:**

```bash
sudo pacman -S --needed shaderc
```

```bash
tests/run_tests.sh
```

Either way, the last line should be `all 21 presets ok`.

## 4. Play with Clarity

```bash
cd dist/ClearSkies-linux
```

```bash
./clearskies.sh --game ~/Games/Savage_Wind-0.5.1-alpha.5-x86_64.AppImage --preset serfrost
```

After this first run, `./clearskies.sh` on its own starts the game with the saved settings. To see the
difference, start the AppImage directly, without Clear Skies, for the original look.

| Command | What it does |
| --- | --- |
| `./clearskies.sh` | Start with the saved preset and game |
| `./clearskies.sh --preset filmic` | Choose a preset (it's saved) and start |
| `./clearskies.sh --preset off` | Start with the game's own look |
| `./clearskies.sh --list` | List the presets |
| `./clearskies.sh --edit` | Open `clearskies.ini` in your editor |
| `./clearskies.sh --preset filmic --no-play` | Only save the choice |
| `./clearskies.sh -- --config` | Anything after `--` goes to Savage Wind |

## 5. Check the log

```bash
cat clearskies.log
```

A working run looks like this:

```
Clear Skies loaded into /tmp/.mount_…/usr/bin/savagewind
preset: serfrost (Cemu's "Serfrost's Preset (Recommended)")
watching shaders on a new Vulkan device
Clarity compiled (… bytes of SPIR-V)
Clarity replaces the tone-mapping composite (shader 1)
```

If there's no `using shaderc …` line, the layer used the shaderc inside the AppImage, which is the best case.

## If something goes wrong

| What you see | What it means |
| --- | --- |
| No `clearskies.log` at all | The layer didn't load. Keep the output of `./clearskies.sh` |
| `libshaderc_shared.so.1 not found` | The layer couldn't find shaderc. Install it: `sudo pacman -S --needed shaderc` |
| `loaded into`, but never `replaces the tone-mapping composite` | Your Savage Wind version builds that shader differently |
| Savage Wind opens its own setup screen | Finish Savage Wind's setup first, then start Clear Skies again |
| `failed to compile` | A value in `clearskies.ini` can't be used. The message says which one |

## What to send back

- `clearskies.log` from `dist/ClearSkies-linux/`
- The output of `./clearskies.sh` in the terminal
- Your distribution, GPU, and driver (`vulkan-radeon`, `nvidia-utils`, …)
- The preset you used, and a screenshot if something looks wrong

**Never send game files, dumps, keys or saves.**
