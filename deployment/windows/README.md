# Windows: Qt Creator Release → Deploy → installer

## Prerequisites (once)

Use the accepted **Desktop Qt 6.10.3 MinGW 64-bit** kit (GCC 13.1), CMake/Ninja,
Python >=3.11, verified marine/SDL2 dependencies and Qt's matching Sources component.
Install Inno Setup 6 separately if absent. Default compiler location is
`C:/Program Files (x86)/Inno Setup 6/ISCC.exe`; a custom location is selected using
`RAYNMEA_ISCC`. This workflow does not install prerequisites or require elevation.
LLVM and MSVC are not qualified deployment kits for this application.

## Qt Creator

1. Open `CMakeLists.txt` in Qt Creator and select **Desktop Qt 6.10.3 MinGW 64-bit**.
2. In **Projects → Build Settings**, choose **CMake Release** (or an existing
   configuration with Build type **Release**). Keep the accepted dependency root.
3. Run CMake to refresh targets after updating deployment source. There is no
   need to set `RAYNMEA_DEPLOYMENT=ON` or change existing development options.
4. Under **Build Steps → CMake Build → Details**, check **Deploy**, then uncheck
   `all`. Confirm the displayed build command ends in `--target Deploy` only.
   Checking Deploy first avoids Qt Creator restoring `all` when no target is selected.
   This is the CMake **Build** target, not Qt Creator's
   separate device Deploy Settings/Run action.
5. Build the project (Ctrl+B). Read **Compile Output** and require success.
6. Locate the directory shown in **Build directory** on this same settings page.
   Inspect `<that-directory>/deploy/RayNmeaSimulator/`.
7. Retrieve `<that-directory>/deploy/installer/RayNmeaSimulator-0.1.0-Windows-x64-Setup.exe`
   and its `.sha256` sidecar. The generated Inno configuration is at
   `<that-directory>/deployment/windows/RayNmeaSimulator.iss`.

Deploy builds a separate curated Release application inside `deployment/build/`.
The ordinary development executable, kits and authoring configuration remain.
No Debug/test binary or additional owner Template JSON is packaged. Debug Deploy
fails with an actionable Release-only message. Do not select `all` if your goal
is an installer; `all` remains an ordinary application build.

## Command line

Open a shell with the accepted kit's compiler, CMake and Ninja on PATH. From the
source root, with `$qt` set to the installed `mingw_64` prefix and `$deps` to an
already verified native dependency stage:

```powershell
$qt = 'C:/Qt6.5/6.10.3/mingw_64'  # adjust for your Qt installation
$deps = (Resolve-Path 'build/m252/deps').Path # or your qualified native stage
cmake --preset dev-release "-DCMAKE_PREFIX_PATH=$qt" "-DDEPS_ROOT=$deps"
if ($LASTEXITCODE -ne 0) { throw 'Configure failed' }
cmake --build build/cmake/dev-release --target Deploy
if ($LASTEXITCODE -ne 0) { throw 'Deploy failed' }
```

For an existing Qt Creator Release build, configure is unnecessary:

```powershell
cmake --build '<actual-Qt-Creator-Release-build-directory>' --target Deploy
```

`python -B deployment/deploy.py --build '<Release-build>' --cmake '<path-to-cmake.exe>'`
is the equivalent source entry point. Run from the same kit environment.

## Inspect and test

The stage contains the executable, Qt/SDL2/MinGW runtime, plugins, manifests and
complete notices. `manifest.json` must match staged content. Require the latest
`deploy/deployment-result.json` to say `succeeded`; verify the installer SHA-256
with `Get-FileHash` against the sidecar. Previous managed outputs are retained
internally, not left at current paths after an adapter/build/package failure.

Test the installer on a separate Windows x64 machine. It remains per-user,
keeps its established AppId and upgrade directory, offers an unchecked desktop
shortcut and does not launch the app automatically. Uninstall removes managed
application files, not settings, credentials or owner documents. Do not overwrite
an owner installation for a test. No public release, signing or upload is performed.
See [the overview](../README.md) for output-state and publication boundaries.
