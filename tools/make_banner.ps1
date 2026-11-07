# The README's banner (docs/art/banner.png, 1600x480): a clear sky with the sun and clouds, the icon and the wordmark.
# Drawn with .NET's System.Drawing (built into Windows), lettering in Fira Sans (OFL, third_party/fonts/FiraSans).
#   powershell -ExecutionPolicy Bypass -File tools\make_banner.ps1
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$root = Split-Path -Parent $PSScriptRoot
$W = 1600; $H = 480

$fonts = New-Object System.Drawing.Text.PrivateFontCollection
$fonts.AddFontFile("$root\third_party\fonts\FiraSans\FiraSans-SemiBold.ttf")
$fonts.AddFontFile("$root\third_party\fonts\FiraSans\FiraSans-Medium.ttf")
$fira = $fonts.Families | Where-Object { $_.Name -eq 'Fira Sans SemiBold' } | Select-Object -First 1
$firaMed = $fonts.Families | Where-Object { $_.Name -eq 'Fira Sans Medium' } | Select-Object -First 1
if (-not $fira) { $fira = $fonts.Families[0] }
if (-not $firaMed) { $firaMed = $fira }

$bmp = New-Object System.Drawing.Bitmap $W, $H
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = 'AntiAlias'
$g.InterpolationMode = 'HighQualityBicubic'
$g.TextRenderingHint = 'AntiAliasGridFit'
function C($a, $r, $gr, $b) { [System.Drawing.Color]::FromArgb($a, $r, $gr, $b) }

# Sky
$sky = New-Object System.Drawing.Drawing2D.LinearGradientBrush (New-Object System.Drawing.Point 0, 0), (New-Object System.Drawing.Point 0, $H), (C 255 0x1f 0x6f 0xc8), (C 255 0xb4 0xdd 0xf7)
$g.FillRectangle($sky, 0, 0, $W, $H)

# Sun glow: a radial gradient, warm in the middle fading to nothing
$sx = 1240; $sy = 165; $gr = 330
$path = New-Object System.Drawing.Drawing2D.GraphicsPath
$path.AddEllipse($sx - $gr, $sy - $gr, 2 * $gr, 2 * $gr)
$glow = New-Object System.Drawing.Drawing2D.PathGradientBrush $path
$glow.CenterColor = C 210 0xff 0xf0 0xc8
$glow.SurroundColors = @(C 0 0xff 0xf0 0xc8)
$blend = New-Object System.Drawing.Drawing2D.Blend 4
$blend.Factors = [single[]]@(0, 0.25, 0.7, 1)
$blend.Positions = [single[]]@(0, 0.45, 0.8, 1)
$glow.Blend = $blend
$g.FillEllipse($glow, $sx - $gr, $sy - $gr, 2 * $gr, 2 * $gr)
$g.FillEllipse((New-Object System.Drawing.SolidBrush (C 255 0xff 0xec 0xb0)), $sx - 78, $sy - 78, 156, 156)

# Clouds: soft, from stacked translucent puffs
function Cloud($x, $y, $s, $alpha) {
    $puffs = @(@(0, 0, 1.0), @(0.85, -0.38, 0.95), @(1.75, -0.1, 1.05), @(2.6, 0.12, 0.8), @(1.3, 0.25, 1.1))
    foreach ($soft in 3, 2, 1, 0) {
        $a = if ($soft -eq 0) { $alpha } else { [int]($alpha * 0.09) }
        $b = New-Object System.Drawing.SolidBrush (C $a 0xf8 0xfc 0xff)
        foreach ($p in $puffs) {
            $r = $s * $p[2] + $soft * 6
            $g.FillEllipse($b, [single]($x + $s * $p[0] - $r), [single]($y + $s * $p[1] - $r * 0.72), [single](2 * $r), [single](1.44 * $r))
        }
    }
}
Cloud 1215 392 58 235
Cloud 1395 425 74 255
Cloud 905 452 38 190
Cloud 1450 262 32 170

# Left side darkened a touch so the white lettering reads
$shade = New-Object System.Drawing.Drawing2D.LinearGradientBrush (New-Object System.Drawing.Point 0, 0), (New-Object System.Drawing.Point 1000, 0), (C 120 0x0b 0x2a 0x52), (C 0 0x0b 0x2a 0x52)
$shade.WrapMode = [System.Drawing.Drawing2D.WrapMode]::TileFlipX; $g.FillRectangle($shade, 0, 0, 998, $H)

# Icon with a soft shadow
$icon = [System.Drawing.Image]::FromFile("$root\docs\icon.png")
$s = 232; $ix = 84; $iy = [int](($H - $s) / 2)
for ($i = 8; $i -ge 1; $i--) {
    $sh = New-Object System.Drawing.SolidBrush (C 10 0x05 0x18 0x33)
    $rp = New-Object System.Drawing.Drawing2D.GraphicsPath
    $o = $i * 3
    $rr = 50 + $o
    $x0 = $ix - $o + 6; $y0 = $iy - $o + 12; $w0 = $s + 2 * $o
    $rp.AddArc($x0, $y0, $rr, $rr, 180, 90); $rp.AddArc($x0 + $w0 - $rr, $y0, $rr, $rr, 270, 90)
    $rp.AddArc($x0 + $w0 - $rr, $y0 + $w0 - $rr, $rr, $rr, 0, 90); $rp.AddArc($x0, $y0 + $w0 - $rr, $rr, $rr, 90, 90)
    $g.FillPath($sh, $rp)
}
$g.DrawImage($icon, $ix, $iy, $s, $s)

# Wordmark
$tx = $ix + $s + 60
$title = New-Object System.Drawing.Font $fira, 132, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
$sub = New-Object System.Drawing.Font $firaMed, 42, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
$shadow = New-Object System.Drawing.SolidBrush (C 70 0x06 0x1d 0x3d)
$white = New-Object System.Drawing.SolidBrush (C 255 0xff 0xff 0xff)
$soft = New-Object System.Drawing.SolidBrush (C 235 0xf0 0xf7 0xfd)
$g.DrawString('Clear Skies', $title, $shadow, $tx + 3, 128 + 5)
$g.DrawString('Clear Skies', $title, $white, $tx, 128)
$g.DrawString("Cemu's Clarity presets for Savage Wind", $sub, $shadow, $tx + 8, 290 + 3)
$g.DrawString("Cemu's Clarity presets for Savage Wind", $sub, $soft, $tx + 6, 290)

New-Item -ItemType Directory -Force "$root\docs\art" | Out-Null
$bmp.Save("$root\docs\art\banner.png", [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose(); $icon.Dispose()
"make_banner: docs\art\banner.png ($W x $H)"
