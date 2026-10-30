param(
    [string]$BuildRoot,
    [string]$QtRoot,
    [string]$MinGWRoot,
    [string]$DepsRoot,
    [switch]$Smoke
)
$ErrorActionPreference = 'Stop'
$appRoot = Split-Path -Parent $PSScriptRoot
if (!$BuildRoot) { $BuildRoot = Join-Path $appRoot 'build/cmake/route-acceptance' }
$arguments = @((Join-Path $PSScriptRoot 'cmake-dev.py'), 'run', '--build-root', $BuildRoot,
               '--target', 'route_acceptance_fixture', '--platform', 'windows')
if ($QtRoot) { $arguments += @('--qt-root', $QtRoot) }
if ($MinGWRoot) { $arguments += @('--compiler-root', $MinGWRoot) }
if ($DepsRoot) { $arguments += @('--deps-root', $DepsRoot) }
if ($Smoke) { $arguments += '--smoke' }
# CMake, Ninja, Python and the selected compiler must be on PATH.
& python @arguments
if ($LASTEXITCODE -ne 0) { throw "Route acceptance failed with exit code $LASTEXITCODE" }
