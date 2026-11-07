// Clear Skies: the shader around Cemu's Clarity code.
//
// Cemu's "Clarity" for Breath of the Wild (cemu_graphic_packs, BreathOfTheWild/Enhancements, shader
// 37040a485a29d54e: Clarity GFX by Jamie and Kiri with Crementif; presets by their named creators; CC0 1.0). On the
// Wii U it replaces the game's HDR -> LDR composite; here it replaces the same pass of the Switch build as Savage
// Wind translates it (runtime/nvn/maxwell/translate.c): fragment resources in set 1, t0 (HDR scene) at binding 32,
// t1 (bloom) at binding 33, the screen coordinates in v0 at location 0, the colour out at location 0.
//
// The source given to shaderc is: #version, the preset's settings (cemu_presets.h, k_params), k_interface,
// Cemu's functions (cemu_presets.h, k_cemu_functions), k_main.
#pragma once

#include "cemu_presets.h"

// After the settings. V0_INTERP is defined to the original shader's interpolation for v0 (it must match).
static const char k_interface[] =
    "layout(set = 1, binding = 32) uniform sampler2D t0;  // HDR scene (Cemu: textureUnitPS1)\n"
    "layout(set = 1, binding = 33) uniform sampler2D t1;  // bloom (Cemu: textureUnitPS0)\n"
    "layout(location = 0) V0_INTERP in vec4 v0;           // Cemu: passParameterSem0\n"
    "layout(location = 0) out vec4 c0;                    // Cemu: passPixelColor0\n"
    "vec4 hdr_tex(sampler2D tex, vec2 p) { return min(texture(tex, p), vec4(10000.0)); }\n";

// Cemu's main(), with Savage Wind's names. Clamped to [0, 1] and alpha 0, as the game's own composite writes it.
static const char k_main[] =
    "void main() {\n"
    "  vec3 bloom = texture(t1, v0.xy).xyz;\n"
    "#if (adjust_bloom == 1)\n"
    "  bloom *= bloomFactor;\n"
    "#endif\n"
    "#if (HDRpassing == 1)\n"
    "  vec3 color = HDRPass(t0, v0.xy).xyz;\n"
    "#else\n"
    "  vec3 color = hdr_tex(t0, v0.xy).xyz;\n"
    "#endif\n"
    "#if (lumapassing == 1)\n"
    "  color += vec3(lumasharping(t0, v0.xy));\n"
    "#endif\n"
    "  color += bloom;\n"
    "#if (Tone_map == -1)\n"
    "  color = clamp(color * Exposure, 0.0, 1.0);\n"
    "#elif (Tone_map == 0)\n"
    "  color = BotWToneMap(color);\n"
    "#elif (Tone_map == 1)\n"
    "  color = ReshadeToneMap(color);\n"
    "#elif (Tone_map == 2)\n"
    "  color = linearToneMapping(color);\n"
    "#elif (Tone_map == 3)\n"
    "  color = simpleReinhardToneMapping(color);\n"
    "#elif (Tone_map == 4)\n"
    "  color = lumaBasedReinhardToneMapping(color);\n"
    "#elif (Tone_map == 5)\n"
    "  color = whitePreservingLumaBasedReinhardToneMapping(color);\n"
    "#elif (Tone_map == 6)\n"
    "  color = RomBinDaHouseToneMapping(color);\n"
    "#elif (Tone_map == 7)\n"
    "  color = filmicToneMapping(color);\n"
    "#elif (Tone_map == 8)\n"
    "  color = Uncharted2ToneMapping(color);\n"
    "#elif (Tone_map == 9)\n"
    "  color = ACESFilm(color);  // (Cemu's pack tests a misspelt name here, so 9 does nothing there)\n"
    "#endif\n"
    "#if (blacknwhitepass == 1)\n"
    "  color = LevelsPass(color);\n"
    "#endif\n"
    "#if (cmatrix == 1)\n"
    "  color = ColorMatrixPass(color);\n"
    "#endif\n"
    "#if (Tech == 1)\n"
    "  color = TechnicolorPass(color);\n"
    "#endif\n"
    "#if (Techine == 1)\n"
    "  color = Technicolor2(color);\n"
    "#endif\n"
    "#if (post_process == 0)\n"
    "  color = BotWVibrance(color);\n"
    "#endif\n"
    "#if (post_process == 1)\n"
    "  color = Contrasty(color);\n"
    "#endif\n"
    "#if (dpxpass == 1)\n"
    "  color = DPXPass(color);\n"
    "#endif\n"
    "#if (Filmicpass == 1)\n"
    "  color = FilmPass(color);\n"
    "#endif\n"
    "#if (lggpass == 1)\n"
    "  color = LiftGammaGainPass(color);\n"
    "#endif\n"
    "#if (CurvesPss == 1)\n"
    "  color = CurvesPass(color);\n"
    "#endif\n"
    "#if (vibpass == 1)\n"
    "  color = VibrancePass(color);\n"
    "#endif\n"
    "  c0 = vec4(clamp(color, 0.0, 1.0), 0.0);\n"
    "}\n";
