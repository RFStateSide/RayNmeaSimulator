# HISTORICAL: use only with the pre-retirement source/build recorded by this wrapper.
# Current isolated launch: scripts/cmake-dev.py run (see docs/build-cmake.md).
param(
    [string]$QtRoot = $env:QT_ROOT,
    [string]$MinGWRoot = $env:MINGW_ROOT,
    [string]$BuildRoot,
    [string]$Profile,
    [string]$DefaultLayout,
    [string]$SDL2Root,
    [switch]$AcceptanceSmoke
)
$ErrorActionPreference = 'Stop'
$appRoot = Split-Path -Parent $PSScriptRoot
if (!$QtRoot -or !$MinGWRoot) { throw 'Set QT_ROOT and MINGW_ROOT or pass -QtRoot and -MinGWRoot.' }
if (!$BuildRoot) { $BuildRoot = Join-Path $appRoot 'build\m15b2a' }
if (!$Profile) { $Profile = Join-Path $BuildRoot ('acceptance-' + [guid]::NewGuid().ToString('N')) }
if (!$DefaultLayout) { $DefaultLayout = Join-Path $appRoot 'tests\fixtures\defaultLayout.json' }
if (!$SDL2Root) { $SDL2Root = Join-Path $appRoot '.dependencies\SDL2-2.30.9\x86_64-w64-mingw32' }
$exe = Join-Path $BuildRoot 'tests\debug\RayNmeaSimulator-isolated.exe'
foreach ($required in @($exe, "$QtRoot\bin\Qt6Core.dll", "$MinGWRoot\bin\libstdc++-6.dll", "$SDL2Root\bin\SDL2.dll")) {
    if (!(Test-Path -LiteralPath $required)) { throw "Missing required runtime file: $required" }
}
$Profile = [IO.Path]::GetFullPath($Profile)
# Test builds use explicit private INI settings and layout paths.
if ($Profile -eq [IO.Path]::GetFullPath($env:USERPROFILE)) { throw 'Profile must be an isolated directory.' }
$layouts = Join-Path $Profile '.raynmeasimulator\layouts'
$settings = Join-Path $Profile 'settings\RFStateSide'
New-Item -ItemType Directory -Force -Path $layouts, $settings | Out-Null
$copy = Join-Path $layouts 'defaultLayout.json'
if (!(Test-Path -LiteralPath $copy)) {
    if (!(Test-Path -LiteralPath $DefaultLayout)) { throw 'Provide -DefaultLayout with a layout to copy.' }
    Copy-Item -LiteralPath $DefaultLayout -Destination $copy
}
$ini = Join-Path $settings 'RayNmeaSimulator.ini'
if (!(Test-Path -LiteralPath $ini)) {
    @'
[Preference]
StyleSheetName=darkeum
UnitsTemp=2
UnitsPressure=3
UnitsLatLong=2
[Vessel]
NumEngines=2
EngineProfile=MerCruiser 7.4 MPI
'@ | Set-Content -LiteralPath $ini -Encoding UTF8
}
$env:PATH = "$QtRoot\bin;$MinGWRoot\bin;$SDL2Root\bin;" + $env:PATH
$env:QT_PLUGIN_PATH = "$QtRoot\plugins"
$env:QT_QPA_PLATFORM = 'windows'
$env:RAYNMEA_TEST_HOME = $Profile
if ($AcceptanceSmoke) { $env:RAYNMEA_ACCEPTANCE_SMOKE = '1' }
else { Remove-Item Env:RAYNMEA_ACCEPTANCE_SMOKE -ErrorAction SilentlyContinue }
Write-Host "Standalone executable: $exe"
Write-Host "Isolated settings/layouts: $Profile"
Write-Host 'Arguments: --no-nmea'
Push-Location (Split-Path -Parent $exe)
try { & $exe --no-nmea; $result = $LASTEXITCODE } finally { Pop-Location }
exit $result
