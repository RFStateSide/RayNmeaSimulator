# HISTORICAL: use only with the pre-retirement source/build recorded by this wrapper.
# Current isolated launch: scripts/cmake-dev.py run (see docs/build-cmake.md).
param(
    [string]$QtRoot = 'C:/Qt6.5/6.7.3/mingw_64',
    [string]$MinGWRoot = 'C:/Qt6.5/Tools/mingw1120_64',
    [string]$BuildRoot,
    [switch]$AcceptanceSmoke
)
$ErrorActionPreference = 'Stop'
$appRoot = Split-Path -Parent $PSScriptRoot
if (!$BuildRoot) { $BuildRoot = Join-Path $appRoot 'build/m17a/windows' }
$exe = Join-Path $BuildRoot 'tests/debug/RayNmeaSimulator-isolated.exe'
if (!(Test-Path -LiteralPath $exe)) { throw "Build M1.7A isolated target first: $exe" }
$profile = Join-Path $BuildRoot ('acceptance-' + [guid]::NewGuid().ToString('N'))
$settings = Join-Path $profile 'settings/RFStateSide'
New-Item -ItemType Directory -Force -Path $settings | Out-Null
@'
[Preference]
StyleSheetName=ConsoleStyle
[Vessel]
NumEngines=2
EngineProfile=MerCruiser 7.4 MPI
'@ | Set-Content -LiteralPath (Join-Path $settings 'RayNmeaSimulator.ini') -Encoding UTF8
$env:PATH = "$QtRoot/bin;$MinGWRoot/bin;$appRoot/.dependencies/SDL2-2.30.9/x86_64-w64-mingw32/bin;" + $env:PATH
$env:QT_PLUGIN_PATH = "$QtRoot/plugins"
$env:QT_QPA_PLATFORM = 'windows'
$env:RAYNMEA_TEST_HOME = [IO.Path]::GetFullPath($profile)
Remove-Item Env:RAYNMEA_TEST_ENGINE_PROFILES -ErrorAction SilentlyContinue
if ($AcceptanceSmoke) { $env:RAYNMEA_ACCEPTANCE_SMOKE = '1' }
else { Remove-Item Env:RAYNMEA_ACCEPTANCE_SMOKE -ErrorAction SilentlyContinue }
Write-Host "M1.7A standalone executable: $exe"
Write-Host "Isolated profile: $profile"
Write-Host 'Arguments: --no-nmea; compiled resource defaults; no resource/layout copies'
& $exe --no-nmea
exit $LASTEXITCODE
