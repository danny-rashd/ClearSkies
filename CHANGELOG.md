# Changelog

## Unreleased

- Linux: the layer builds as `libclearskies_layer.so` for Savage Wind's native AppImage. It uses the shaderc the
  AppImage ships, and falls back to the system's.
- Linux: `clearskies.sh`, a launcher script with the same options as ClearSkies.exe. It also works as Steam launch
  options (`clearskies.sh %command%`) for the Steam Deck.
- `build.sh` and `tests/run_tests.sh` build and test the Linux version. All 21 presets pass the layer test on
  Ubuntu 24.04 with Mesa's software Vulkan driver. It hasn't been tried in the game on Linux yet.

## 0.1.0 (2026-11-07)

The first release.

- Cemu's Clarity for Breath of the Wild on Savage Wind, as a Vulkan layer: the game's tone-mapping shader is
  replaced with Clarity, and nothing in Savage Wind's folder is changed.
- All 20 presets from Cemu's graphic pack, and all 65 of Clarity's settings adjustable in `clearskies.ini`.
- ClearSkies.exe, a themed launcher: choose a look, point it at `savagewind.exe`, press Play. It also has
  command-line options, and warns when Savage Wind isn't set up yet.
- `clearskies.log` for troubleshooting.

Tested in the game on one PC (AMD Radeon RX 9070 XT, Windows 11, HDR off) with Serfrost's preset, and every preset
automatically on that GPU. NVIDIA, Intel, HDR output, Linux and Steam Deck are untested.
