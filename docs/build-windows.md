# Windows CMake development

Qualified native kit: Qt6.10.3 mingw_64, MinGW GCC13.1.0 x64, CMake3.30.5,
Ninja1.12.1, SDL2 2.30.9 MinGW SDK. Use [the CMake guide](build-cmake.md)
for fresh-clone provisioning, Debug/Release presets, focused tests and helpers.

```powershell
$env:PATH = "$env:QT_ROOT/bin;$env:MINGW_ROOT/bin;<CMake-bin>;<Ninja-directory>;" + $env:PATH
python scripts/dependencies.py acquire --with-sdl2
python scripts/dependencies.py verify --with-sdl2
cmake --preset dev-debug "-DCMAKE_PREFIX_PATH=$env:QT_ROOT"
cmake --build --preset dev-debug --target RayNmeaSimulator
cmake --preset dev-release "-DCMAKE_PREFIX_PATH=$env:QT_ROOT"
cmake --build --preset dev-release --target RayNmeaSimulator
```

Set QT_ROOT/MINGW_ROOT to matching installed SDK prefixes; replace tool-path
placeholders. Runtime PATH also needs .dependencies/SDL2-2.30.9/x86_64-w64-mingw32/bin.
No toolchain upgrade/download occurs during configuration. Do not mix MinGW/MSVC,
x86/x64 or Qt versions. The Windows icon/RC resource is part of the CMake target.

Accepted Qt Creator Debug/Release build/run, GDB and focused CTest remain the
developer workflow. Preserve local kit/run settings. For settings-safe manual
launch use cmake-dev.py run with RayNmeaSimulator-isolated; ordinary production
builds use normal preferences. --no-nmea alone does not isolate those preferences.
Windows route_acceptance_fixture remains memory-only and supports --smoke.

Software qualification is not physical CAN qualification. Keep Actisense Release
RX-only/Debug experimental restrictions and explicit Enable Sending. Installers,
DLL/plugin deployment and clean-machine testing remain Issue #18. Earlier Windows
toolchain/build instructions are preserved under history, not current support claims.
