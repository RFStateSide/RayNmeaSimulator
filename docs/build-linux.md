# Ubuntu CMake development

Qualified environment: Ubuntu26.04.1 x86_64, Qt6.10.3 gcc_64, GCC15.2,
native SDL2 2.32.10, CMake3.30.5 and Ninja1.12.1. Native Qt Creator Debug/Release
build/run, GDB and focused CTest are owner accepted. See [CMake](build-cmake.md).

Provide matching Qt modules, native compiler, Git/Python3, pkg-config and SDL2
development files. Put the selected Qt/CMake/Ninja/compiler on PATH. Packages are
prerequisites; no script installs/upgrades them or changes CAN permissions/services.

```sh
python3 scripts/dependencies.py acquire
python3 scripts/dependencies.py verify
cmake --preset dev-debug -DCMAKE_PREFIX_PATH="$QT_ROOT"
cmake --build --preset dev-debug --target RayNmeaSimulator
cmake --preset dev-release -DCMAKE_PREFIX_PATH="$QT_ROOT"
cmake --build --preset dev-release --target RayNmeaSimulator
python3 scripts/cmake-dev.py run --build-root build/manual-tests --qt-root "$QT_ROOT" --target RayNmeaSimulator-isolated --platform xcb
```

QT_ROOT is the selected native prefix. New clones use clone-local .dependencies;
existing DEPS_ROOT/individual overrides stay local. The accepted historical Ubuntu
development checkout name and its working dependency roots remain valid. Do not
rename/migrate them for this source change. Existing binaries/IDE settings remain.
Native Wayland qualification and installed-runtime completeness are separate from
accepted desktop development; record the actual backend used for a new test.

## Desktop identity and staging

The ID com.rfstateside.RayNmeaSimulator matches main.cpp and StartupWMClass.
Development builds do not register a desktop entry automatically. If explicitly
desired, register an existing executable per user with:

```sh
python3 scripts/register-linux-desktop.py --executable build/cmake/dev-debug/RayNmeaSimulator
```

The helper retains backup/no-op/path-validation behavior and never starts the app.
Existing modified owner launchers/icons are preserved. Do not run it merely to
validate a source build. Focused disposable tests are linux_desktop_registration_tests.

CMake preserves the existing fixed /opt executable and /usr launcher/icon install
paths. Verify to a NEW disposable staging directory only:

```sh
DESTDIR="$PWD/build/install-stage" cmake --install build/cmake/dev-release
```

Check opt/RayNmeaSimulator/bin/RayNmeaSimulator, usr/share/applications/
com.rfstateside.RayNmeaSimulator.desktop and usr/share/pixmaps/
RayNmeaSimulator_1024.png below that root. Exec/TryExec and Icon match these
established locations. --prefix does not relocate these fixed rules.
Never stage over owner data or install into the live /opt or /usr for this check.
This is not Qt/SDL bundling or an Ubuntu package; deployment remains Issue #18.
SocketCAN must already be configured by its owner; builds/tests do not change it.
