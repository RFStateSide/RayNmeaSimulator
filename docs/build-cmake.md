# CMake development and focused CTest

## Prerequisites and fresh clone

Provide Git, Python 3, CMake 3.24+, Ninja, a C++17 native compiler and matching
Qt 6.10 desktop SDK. Modules: Core, Gui, Widgets, Network, SerialPort, Xml, Charts,
Location, Positioning, WebSockets, Multimedia; Test for test/helper builds.
Windows uses 64-bit MinGW (qualified Qt 6.10.3/GCC 13.1.0). Linux uses native
GCC, pkg-config and SDL2 development files. Match architecture and runtime ABI.
Put compiler, Qt bin, CMake and Ninja on PATH. No implicit package installation.

```text
git clone https://github.com/RFStateSide/RayNmeaSimulator.git
cd RayNmeaSimulator
python scripts/dependencies.py acquire --with-sdl2
python scripts/dependencies.py verify --with-sdl2
```

Use authorized GitHub access while private. On Linux use python3 and omit
--with-sdl2. The clone-local ignored .dependencies stage contains exact pinned
marine checkouts and, on Windows, the verified SDL2 2.30.9 x64 MinGW SDK/archive.
See [dependency integrity and recovery](DEPENDENCIES.txt). Normal configure/build
only verifies; it never downloads, resets, upgrades or repairs dependencies.
Machine-local overrides belong in ignored CMakeUserPresets.json or IDE settings.
Historical milestone paths are not needed for a fresh clone.

## Application builds

```text
cmake --preset dev-debug -DCMAKE_PREFIX_PATH=<Qt-prefix>
cmake --build --preset dev-debug --target RayNmeaSimulator
cmake --preset dev-release -DCMAKE_PREFIX_PATH=<Qt-prefix>
cmake --build --preset dev-release --target RayNmeaSimulator
```

Use separate build/cmake/dev-debug and dev-release directories. Release presets
retain -O2 -DNDEBUG. Optional cache roots: DEPS_ROOT, NMEA2000_ROOT, NMEA0183_ROOT,
SOCKETCAN_ROOT, SDL2_ROOT (Windows), SDL2_ARCHIVE, Python3_EXECUTABLE.
Fresh clones default to their own .dependencies. Native Linux SDL2 uses pkg-config.
Existing individual cached overrides do not follow later DEPS_ROOT changes:
inspect them or use a new build directory; do not reset old installations.

Application target/output is RayNmeaSimulator[.exe] in the selected directory.
Windows runtime needs Qt bin, compiler runtime and SDL2 bin on PATH; Linux needs
the matching Qt libraries/plugins. No binary deployment or runtime bundling is
implied. Ordinary production runs use normal user settings. --no-nmea prevents
marine initialization but does not isolate production preferences/credentials.

## Focused tests

```text
cmake --preset tests-debug -DCMAKE_PREFIX_PATH=<Qt-prefix>
cmake --build --preset tests-debug --target ray_tests_cmake
ctest --preset tests-debug -R "^(cmake_parity_tests|cmake_startup_smoke)$" --output-on-failure
cmake --build --preset tests-debug --target ray_tests_navigation
ctest --preset tests-debug -L navigation --output-on-failure
```

BUILD_TESTING defaults OFF for ordinary application builds. Dedicated Tests Debug
uses ON. All tests are excluded from the normal all target: discovery is not a
build or pass. Build selected targets/groups first. CTest REQUIRED_FILES records
executable prerequisites, including both settings probes. Tests/CMakeLists.txt
is the sole regression/group inventory. Platform-specific timing and SocketCAN
targets retain their availability. See [tests](../tests/README.md).

For discoverable selection without a second test allowlist, use the portable
developer runner in a NEW dedicated directory (it will not repurpose IDE builds):

```text
python scripts/cmake-dev.py list --build-root build/dev-tests --qt-root <Qt-prefix>
python scripts/cmake-dev.py test --build-root build/dev-tests --qt-root <Qt-prefix> --suites cmake_parity_tests cmake_startup_smoke
python scripts/cmake-dev.py test --build-root build/dev-tests --qt-root <Qt-prefix> --label navigation
python scripts/cmake-dev.py build --build-root build/dev-tests --qt-root <Qt-prefix> --target create_test_layout network_can_peer
```

Use python3 on Linux. Windows can add --compiler-root <MinGW-prefix>. --deps-root
and --sdl-root select existing verified installations. --config Release selects
a separate new directory; --jobs defaults2. --match selects names by regex;
--suites uses exact registered names. test requires explicit selection and
builds executable prerequisites from CTest JSON. Python-only tests need no C++
build. A zero-match/unknown selection fails. Do not select all merely because
entries are listed. The runner never calls dependency acquisition.

The existing run-cmake-test.py launcher creates fresh profile/work/evidence
directories and bounded test runs. It removes inherited live/private replay and
external evidence opt-ins, copies only embedded public routes, starts only owned
loopback virtual bridges where needed, and cleans up their keys/processes.
QtTest XML and suite outputs are retained in test-evidence. Internal skips are
not hardware passes. Independent NMEA0183 sentence validation remains included.

## Isolated manual helpers

```text
python scripts/cmake-dev.py run --build-root build/manual-tests --qt-root <Qt-prefix> --target RayNmeaSimulator-isolated --platform <windows-or-xcb>
python scripts/cmake-dev.py run --build-root build/manual-tests --qt-root <Qt-prefix> --target RayNmeaSimulator-isolated --smoke --platform offscreen
```

Manual run builds the isolated application and uses the existing isolation
launcher, --no-nmea and a fresh profile. It is bounded to one hour; close the
window normally when finished. --smoke runs the existing real-entry startup
smoke check instead of an interactive session. No normal profile is opened.
The source root is inferred from the runner, not a historical build directory.
On Windows route_acceptance_fixture is also available through run, optionally
--smoke; scripts/run-m239-route-acceptance.ps1 is a thin convenience wrapper with
-BuildRoot/-QtRoot/-MinGWRoot/-DepsRoot. It uses memory ports/credentials and
volatile HKCU isolation. This fixture is Windows-only; Linux route_service_tests
cover the synthetic route service. network_can_peer is an explicitly built helper,
not automatically started or connected by this runner.

## Qt Creator

Open top-level CMakeLists.txt. Use the accepted native Qt kit and three separate
configurations: CMake Debug, CMake Release (testing OFF), CMake DebugTests (ON).
Keep separate build directories and existing local dependency roots. Configure
prefix/compiler/Ninja explicitly when a generic preset lacks SDK information;
do not select an unconfigured generic preset kit instead of the qualified kit.
Build/run target RayNmeaSimulator for ordinary app development; native GDB for
C++ Widgets debugging. Test targets/groups are selected in Build Steps.

Project Settings > Testing: project-specific CTest only, no QtTest duplicate
discovery and no automatic tests after build. Open Edit > Tests navigation.
After switching from testing OFF, CTest(none) may need CTest off/on in project
settings or a project reopen. Clear filter, Deselect All and explicitly check
desired registrations on every reopen: hidden checkboxes can reset to checked.
Build first, then Run Selected Tests Without Deployment. Test Results and raw
output show exits/failures/skips. LastTest.log alone can mislabel Not Run entries;
use summary/diagnostics and retained result.json/QtTest output as well.
Accepted machine-local IDE settings are not rewritten by this source migration.

## Templates and Linux staging

Debug bundled-source authoring recognizes the CMake project/catalog. Add/edit/
rename/delete uses existing validation, confirmations and backups. Reconfigure,
rebuild and restart: a running compiled snapshot is immutable. Release bundled
Templates remain read-only. Isolated builds embed five curated public examples;
production enumerates the selected source catalog. Review authored JSON privacy
before distributing a binary; do not publish local owner Templates accidentally.

Minimal Linux install rules preserve the established fixed paths:
/opt/RayNmeaSimulator/bin/RayNmeaSimulator,
/usr/share/applications/com.rfstateside.RayNmeaSimulator.desktop,
/usr/share/pixmaps/RayNmeaSimulator_1024.png.
Use DESTDIR to stage into a NEW private directory, never the real system here:

```sh
DESTDIR="$PWD/build/install-stage" cmake --install build/cmake/dev-release
```

These three paths deliberately preserve legacy semantics and do not relocate
through --prefix. No Qt/SDL/runtime bundling, installer, CPack or service setup
is provided. Clean-machine packaging and relocation are separate Issue #18 work.
The independent register-linux-desktop.py helper remains available for explicit
per-user registration of an existing executable; it is not run by configure/build.

Historical project recipes/instructions are archived in docs/history. Existing
local configurations/old outputs and installed SDK tools remain preserved.

## macOS ARM64 build enablement (M2.46B; qualification incomplete)

The milestone-directory commands below reproduce M2.46B evidence only. Normal
Qt Creator development uses the stable M2.46C layout in the next section.

Use native Apple Silicon, Apple Clang/C++17, CMake 3.24+, Ninja, Python 3 and
Qt 6.10.3 macOS with the modules listed above. The existing BrickSuite toolchain
can be reused by explicit paths; do not alter its sources, SDKs, kits or outputs.
The current build requires explicit `CMAKE_OSX_ARCHITECTURES=arm64`; Intel and
universal builds are outside initial qualification. Deployment target, signing,
notarization, distribution format and app-bundle packaging remain future work.
This milestone builds a native development executable, not a deployable bundle.

Provision marine sources explicitly with `python3 -B scripts/dependencies.py acquire`
only if needed, then verify with `python3 -B scripts/dependencies.py verify`.
SocketCAN remains staged/verified but is compiled only on Linux. Do not use
`--with-sdl2` on macOS: that option still provisions the Windows MinGW SDK.

Native SDL2 uses the same 2.30.9 release version, built locally from a SHA256-pinned
source archive into a private ARM64 static stage. Set `QT_ROOT` to the local Qt
6.10.3 macOS prefix and `QT_TOOLS_ROOT` to its sibling Tools directory. These
machine-specific paths belong in the shell environment or local settings.

```sh
export PATH="$QT_TOOLS_ROOT/CMake/CMake.app/Contents/bin:$QT_TOOLS_ROOT/Ninja:$PATH"
python3 -B scripts/sdl2-macos.py acquire --root build/m246b/sdl2-macos-arm64
python3 -B scripts/sdl2-macos.py verify --root build/m246b/sdl2-macos-arm64
cmake -S . -B build/m246b/debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_PREFIX_PATH="$QT_ROOT" \
  -DSDL2_ROOT="$PWD/build/m246b/sdl2-macos-arm64/install" \
  -DBUILD_TESTING=ON
cmake --build build/m246b/debug --parallel 4 --target RayNmeaSimulator \
  macos_timing_tests macos_rollover_tests cmake_parity_tests \
  cmake_startup_smoke nmea0183_compatibility_tests
ctest --test-dir build/m246b/debug --output-on-failure \
  -R '^(macos_timing_tests|macos_rollover_tests|cmake_parity_tests|cmake_startup_smoke|nmea0183_compatibility_tests|sdl2_macos_provisioning_tests|dependency_provisioning_tests)$'
```

`sdl2-macos.py acquire` is the only native SDL acquisition/build command. It accepts
`--archive` for an already downloaded pinned archive, `--cmake`, `--ninja`, and
`--jobs`; defaults install under `.dependencies/sdl2-macos-arm64`. It never installs
system-wide. A nonempty stage is verified, never repaired or overwritten; choose
a new stage after a failed acquisition. Keep its archive, unmodified source,
installed tree and receipt together. CMake verifies them at configure/build and
rejects a cached SDL package resolving outside that verified stage. Verification
checks exact installed bytes against the local build receipt plus ARM64-only
architecture; this is local integrity evidence, not an upstream binary signature
or a claim of bit-identical builds across toolchain versions.

The pinned NMEA0183 source omits Darwin from its desktop floating-point printf
guard. macOS compiles a generated copy under `marine-macos/` with that one guard
extended to `__APPLE__`. The exact guard must occur once or configure fails.
The pinned checkout and approved patch remain unchanged and fully verified.
No Linux/Windows guard, source, timing or linkage changes. The existing sentence
compatibility test independently checks generated output. Native NMEA timing is
an application-owned steady-clock `millis()` hook with unsigned 32-bit wrap;
tests cover units/progression and deterministic upstream rollover extension.

Tests remain separate `RAYNMEA_TESTING` object/resource builds. On macOS the
launcher supplies an owned canonical TMPDIR so Qt temporary profiles retain
the existing no-symlink rule. Test INI/data paths are disposable; XDG variables
alone do not isolate native preferences or Keychain. The startup smoke uses
the isolated real entry point, `--no-nmea`, five curated Templates and stopped
Sending. It is not a production executable run or interactive GUI acceptance.

SocketCAN/native syscall-wrapper tests and GNU `--wrap` remain Linux-only.
Portable Qt SerialPort, Network CAN and Signal K code remains compiled; hardware,
live-provider and external Sending qualification is not implied. Actisense retains
its Debug experimental TX / Release RX-only policy. macOS credential operations
still report backend unavailable; no normal owner Keychain entries are accessed.
M2.46C covers native Qt Creator/LLDB workflow acceptance; M2.46D covers Keychain
and complete credential isolation. Do not run all discovered tests on macOS
before reviewing their native settings and credential assumptions.

Ubuntu's historical acceptance-checkout directory naming and machine-local
DEPS_ROOT remain **UNRESOLVED**, deferred to installer/
deployment environment preparation. Portable provisioning does not correct them.

## Native macOS Qt Creator development (M2.46C)

Keep dependencies independent of build/evidence directories. For a fresh clone,
explicitly provision and verify from its root with the selected native tools on
PATH (CMake/Ninja/Clang; no package-manager or configure-time downloads):

```sh
python3 -B scripts/dependencies.py acquire
python3 -B scripts/dependencies.py verify
python3 -B scripts/sdl2-macos.py acquire
python3 -B scripts/sdl2-macos.py verify
```

Marine roots are `.dependencies/NMEA2000`, `.dependencies/NMEA0183` and
`.dependencies/NMEA2000_socketCAN`. SDL2 is a separately verified ARM64 static
stage at `.dependencies/sdl2-macos-arm64/install`, with its archive/source/build
receipt retained in the parent. Every normal configuration must resolve these
same clone-local roots. Never point an IDE configuration at milestone evidence,
BrickSuite, another checkout, or a temporary clone. Existing verified archives
may be passed to the explicit SDL acquisition command using `--archive`; this
rebuilds and verifies a new managed installation rather than copying a library.

Use the existing native **Qt 6.10.3 for macOS** kit, Apple Clang ARM64 and LLDB.
Do not change its global generator or unrelated project settings. Configure
Ninja and the following values per project configuration, using local SDK/tool
paths in ignored `.qtcreator/CMakeLists.txt.user` or CMakeUserPresets.json only:

| Qt Creator name | Build directory | CMAKE_BUILD_TYPE | BUILD_TESTING |
| --- | --- | --- | --- |
| CMake Debug | build/qtcreator-macos/debug | Debug | OFF |
| CMake Release | build/qtcreator-macos/release | Release | OFF |
| CMake DebugTests | build/qtcreator-macos/debugtests | Debug | ON |

All three set `CMAKE_OSX_ARCHITECTURES=arm64`, `DEPS_ROOT=<clone>/.dependencies`,
`SDL2_ROOT=<clone>/.dependencies/sdl2-macos-arm64/install`, and the same Qt prefix,
Clang and Ninja paths. Release retains `CMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG`.
The native kit must have a real Qt SDK selected; generic preset kits do not
establish Qt selection. Disable the Qt Creator package-manager/maintenance-tool
providers for these project configurations; dependencies are explicitly staged.

Preparing caches with the equivalent CMake command and importing them is also
possible, but Qt Creator can create a temporary kit during import. Verify the
result against the existing native kit; do not accept a temporary/generic kit
merely because parsing succeeds. The M2.46C ignored project configuration was
seeded with the existing kit association and verified after GUI reload.

Production builds now make a minimal development `RayNmeaSimulator.app`, with
identifier `com.rfstateside.RayNmeaSimulator`, version 0.1.0 and the existing ICNS.
Qt Creator obtains the executable location from CMake. Qt frameworks/plugins
still come from the development SDK; this is not a self-contained deployment.

For controlled native Debug and Release runs, set project-local run arguments:

```text
--no-nmea --isolated-profile <clone>/build/qtcreator-macos/profiles/<configuration>
```

This explicit macOS development option validates an absolute, writable directory
without symlink ancestors, refuses the home/root directory, and requires
`--no-nmea`. Application settings use an explicit INI under that profile with
fallback disabled; application data, cache and legacy-home paths also resolve
there. It is independent of RAYNMEA_TESTING; production compile policy remains
production policy. Ordinary launches without this option retain native settings.
It does not implement Keychain storage or claim isolation of all Qt/OS preference
services. Never populate the profile with real credentials or private captures.

Preservation limitation found during M2.46C: Qt's QFileDialog explicitly uses
the native shared `QtProject` preference store even when application settings
use an INI. The shared `com.qtproject.plist` timestamp changed during GUI work;
its pre-run dialog history was not captured and was not overwritten in an
attempted rollback. Do not claim complete shared-preference preservation from
`--isolated-profile`. Further dialog-isolation acceptance needs an independently
verified approach, such as a separately approved disposable macOS account.
The application already sets `AA_DontUseNativeDialogs` so STOP/F12 follows its
Qt event path. Its Qt file picker was opened/cancelled; native NSOpenPanel
acceptance is not implied and this policy was not changed for macOS.

Build in Qt Creator with Cmd+B (the macOS equivalent of Ctrl+B); run with Cmd+R.
Breakpoint toggle is **F8** on macOS; debugging starts/continues with Cmd+Y.
Verify source/stack/locals and use the debugger Stop control to finish. Project
configuration, run arguments and the native kit association must persist after
closing/reopening the CMake project. Global BrickSuite kits are not modified.

DebugTests: enable project-specific **CTest only**, turn off duplicate QtTest
discovery and automatic tests after build. Build `cmake_parity_tests` and
`cmake_startup_smoke` first. Clear test filters, Deselect All, then choose exactly
those two before Run Selected Tests Without Deployment. Recheck selections after
reopening; refresh CTest discovery with a rescan or project reopen if needed.
`macos_profile_probe` / `macos_development_profile_tests` provide a directly
relevant additional check of production-profile persistence and fail-closed
argument/path handling without GUI, native-store or credential access.

M2.46C found a native accessibility crash after loading a Template and enumerating
selected table children. The application report points into
`QMacAccessibilityElement::accessibilitySelectedChildren`; Qt Creator also crashed
during a locator accessibility read. The installed Qt Cocoa source contains
parent-managed synthesized table elements and cache cleanup matching the
ownership area changed in [Qt's cached-element change](https://github.com/qt/qtbase/commit/b1ed5f656f064e553b33752f8e87d2f5b9553e38).
This is evidence for a Qt accessibility ownership investigation, not proof that
every Layout edit crashes or that the upstream change alone caused it. No SDK,
Qt Creator binary, accessibility service or security setting was patched/disabled.
Retained crash evidence is under build/m246c; native GUI qualification remains
conditional until the selected-table failure is resolved and retested.

M2.46C.2 separately built an isolated native ARM64 Qt 6.10.3 candidate with
the merged [QtBase ownership correction](https://codereview.qt-project.org/c/qt/qtbase/+/772484),
commit `8148c48deb8e4345bacb797cb75c011df6f797fa`, applied without adaptation.
Its native Cocoa regression passed three cycles and 636 assertions; the Debug
application passed the bounded Single Engine instrument-edit and disposable
Layout save/reload workflow without recurrence of the original SDK's crash.
The candidate remains an external engineering artifact, outside the repository
and this source commit; it is not an automatically adopted production runtime.
Full accessibility acceptance remains open: some automation-tree controls/cells
were omitted and some locators became stale or were reported offscreen.
The original SDK remains unchanged. Qt Creator 20.0.2's separate bundled Qt
6.11.2 accessibility issue is not repaired by the application SDK correction.
No production accessibility workaround or accessibility permission change was added.

Keychain integration remains M2.46D. Physical adapters, external Sending, full
regression coverage, minimum deployment target, macdeployqt packaging, signing,
notarization and clean-machine deployment remain unqualified. Ubuntu's historical
checkout naming and machine-local DEPS_ROOT remain **UNRESOLVED**.
