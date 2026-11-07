# Clear Skies: runs build\layer_test.exe once per preset with the layer on, and checks the layer's log.
#   powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1 -SavageWind "C:\Games\Savage Wind"
param([Parameter(Mandatory = $true)][string]$SavageWind)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$dist = Join-Path $root 'dist\ClearSkies'
$test = Join-Path $root 'build\layer_test.exe'
$work = Join-Path $root 'build\test'
New-Item -ItemType Directory -Force $work | Out-Null

$ini = Get-Content (Join-Path $dist 'clearskies.ini') -Raw
$presets = @('off') + ([regex]::Matches($ini, '(?m)^;\s{3}(\w+)\s{2,}') | ForEach-Object { $_.Groups[1].Value } | Where-Object { $_ -ne 'off' })
$env:VK_ADD_LAYER_PATH = $dist
$env:VK_LAYER_PATH = $dist
$env:VK_INSTANCE_LAYERS = 'VK_LAYER_CLEARSKIES_savage_wind'
$failed = 0
foreach ($p in $presets) {
    $testIni = Join-Path $work 'clearskies.ini'
    Set-Content -Encoding ascii $testIni "[clarity]`r`npreset = $p`r`n"
    $env:CLEARSKIES_INI = $testIni
    $out = & $test $SavageWind 2>&1 | Out-String
    $log = Get-Content (Join-Path $work 'clearskies.log') -Raw
    $loaded = $log -match 'loaded into'
    $replaced = ([regex]::Matches($log, 'replaces the tone-mapping composite')).Count
    $ok = $LASTEXITCODE -eq 0 -and $loaded -and ($(if ($p -eq 'off') { $replaced -eq 0 } else { $replaced -eq 1 -and $log -match 'compiled' }))
    if (-not $ok) { $failed++ }
    '{0,-20} {1}' -f $p, $(if ($ok) { 'ok' } else { "FAIL`n$out`n$log" })
}
if ($failed) { "$failed preset(s) failed"; exit 1 } else { "all $($presets.Count) presets ok" }
