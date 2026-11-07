# Clear Skies: build with Zig (zig cc, a complete C toolchain for Windows in one download).
#   powershell -ExecutionPolicy Bypass -File build.ps1 [-Zig C:\path\to\zig.exe]
# Output: dist\ClearSkies\ (the mod, ready to zip), build\layer_test.exe (tests\run_tests.ps1)
param([string]$Zig = '')
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
if (-not $Zig) {
    $cmd = Get-Command zig -ErrorAction SilentlyContinue
    if ($cmd) { $Zig = $cmd.Source }
    elseif (Test-Path "$root\tools\zig\zig.exe") { $Zig = "$root\tools\zig\zig.exe" }
    else { throw 'Zig not found: put it on PATH, in tools\zig\, or pass -Zig <path to zig.exe>' }
}
$target = @('-target', 'x86_64-windows-gnu')
$cflags = @('-O2', '-Wall', '-Wextra', '-Wno-unused-parameter', '-Wno-missing-field-initializers')
New-Item -ItemType Directory -Force "$root\build", "$root\dist\ClearSkies" | Out-Null

function Run([string[]]$a) {
    & $Zig @a
    if ($LASTEXITCODE) { throw "zig $($a -join ' ') failed" }
}

$py = Get-Command py -ErrorAction SilentlyContinue
if ($py) { & $py.Source -I "$root\tools\gen_presets.py"; if ($LASTEXITCODE) { throw 'gen_presets failed' } }

Run (@('cc') + $target + $cflags + @('-shared', '-o', "$root\dist\ClearSkies\clearskies_layer.dll", "$root\src\clearskies_layer.c", '-s'))
Run (@('cc') + $target + $cflags + @('-municode', '-Wl,--subsystem,windows', '-o', "$root\dist\ClearSkies\ClearSkies.exe",
    "$root\src\launcher.c", "$root\src\launcher.rc", '-lcomctl32', '-lcomdlg32', '-lshell32', '-ldwmapi', '-luxtheme', '-lgdi32', '-s'))
Run (@('cc') + $target + $cflags + @('-o', "$root\build\layer_test.exe", "$root\tests\layer_test.c"))
Remove-Item "$root\dist\ClearSkies\*.pdb", "$root\dist\ClearSkies\*.lib" -ErrorAction SilentlyContinue

foreach ($f in 'VkLayer_clearskies.json', 'clearskies.ini', 'LICENSE', 'LICENSES.txt', 'README.md') {
    if (($f -ne 'clearskies.ini') -or -not (Test-Path "$root\dist\ClearSkies\clearskies.ini")) {
        Copy-Item "$root\$f" "$root\dist\ClearSkies\$f" -Force
    }
}
Get-ChildItem "$root\dist\ClearSkies" | Format-Table Name, Length -AutoSize
