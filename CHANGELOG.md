# Changelog

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
