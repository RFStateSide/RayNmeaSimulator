# HISTORICAL: use only with the pre-retirement source/build recorded by this wrapper.
# Current isolated launch: scripts/cmake-dev.py run (see docs/build-cmake.md).
param([switch]$AcceptanceSmoke)
$ErrorActionPreference = 'Stop'
$appRoot = Split-Path -Parent $PSScriptRoot
Write-Host 'M2.1A Windows Debug acceptance; isolated profile; --no-nmea'
& "$PSScriptRoot/run-m15b2a-standalone.ps1" -QtRoot 'C:/Qt6.5/6.7.3/mingw_64' -MinGWRoot 'C:/Qt6.5/Tools/mingw1120_64' -BuildRoot "$appRoot/build/m21a/windows" -SDL2Root "$appRoot/.dependencies/SDL2-2.30.9/x86_64-w64-mingw32" -AcceptanceSmoke:$AcceptanceSmoke
