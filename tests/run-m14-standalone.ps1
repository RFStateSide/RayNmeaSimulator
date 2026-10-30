# HISTORICAL: use only with the pre-retirement source/build recorded by this wrapper.
# Current isolated launch: scripts/cmake-dev.py run (see docs/build-cmake.md).
$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $repository 'build\m14-isolated\debug\RayNmeaSimulator-isolated.exe'
if (-not (Test-Path -LiteralPath $executable)) {
    throw "Build tests/isolated.pro in build/m14-isolated first: $executable"
}
$profile = Join-Path $repository 'build\m14-acceptance-profile'
$layouts = Join-Path $profile '.raynmeasimulator\layouts'
$settingsDirectory = Join-Path $profile 'settings\RFStateSide'
New-Item -ItemType Directory -Path $layouts, $settingsDirectory -Force | Out-Null
$defaultCopy = Join-Path $layouts 'defaultLayout.json'
if (-not (Test-Path -LiteralPath $defaultCopy)) {
    $ownerDefault = Join-Path $env:USERPROFILE '.raynmeasimulator\layouts\defaultLayout.json'
    if (-not (Test-Path -LiteralPath $ownerDefault)) {
        throw 'Copy a default layout into the isolated profile first; no owner files are changed.'
    }
    Copy-Item -LiteralPath $ownerDefault -Destination $defaultCopy
}
$settingsFile = Join-Path $settingsDirectory 'RayNmeaSimulator.ini'
if (-not (Test-Path -LiteralPath $settingsFile)) {
    @'
[Preference]
StyleSheetName=darkeum
UnitsTemp=2
UnitsPressure=3
UnitsLatLong=2
[Vessel]
NumEngines=2
EngineProfile=MerCruiser 7.4 MPI
'@ | Set-Content -LiteralPath $settingsFile -Encoding UTF8
}
$env:PATH = 'C:\Qt6.5\6.7.3\mingw_64\bin;C:\Qt6.5\Tools\mingw1120_64\bin;' + (Join-Path $repository 'libs\SDL2\x86_64-w64-mingw32\bin') + ';' + $env:PATH
$env:QT_PLUGIN_PATH = 'C:\Qt6.5\6.7.3\mingw_64\plugins'
$env:QT_FORCE_STDERR_LOGGING = '1'
$env:RAYNMEA_TEST_HOME = $profile
Write-Host "M1.4 acceptance executable: $executable"
Write-Host "Isolated settings and layouts: $profile"
Write-Host 'Standalone mode: --no-nmea'
& $executable --no-nmea
exit $LASTEXITCODE
