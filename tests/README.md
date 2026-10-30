# Focused regression tests and helpers

The authoritative target, label and executable-prerequisite inventory is
tests/CMakeLists.txt. [CMake development](../docs/build-cmake.md) describes pinned
dependencies, dedicated test directories and Qt Creator. No separate suite allowlist
is maintained. Query CTest for platform-specific availability; discovery is not
execution or full regression qualification. macOS ARM64 build enablement and
focused qualification commands are in docs/build-cmake.md. Full native workflow
and Keychain isolation remain M2.46C/M2.46D work.

```text
python scripts/cmake-dev.py list --build-root build/focused-tests --qt-root <Qt-prefix>
python scripts/cmake-dev.py test --build-root build/focused-tests --qt-root <Qt-prefix> --suites cmake_parity_tests cmake_startup_smoke
python scripts/cmake-dev.py test --build-root build/focused-tests --qt-root <Qt-prefix> --label navigation
```

Use python3 on Linux, selected tools on PATH and --compiler-root for Windows MinGW
when needed. The runner configures only its own new directory, reads CTest JSON,
builds REQUIRED_FILES targets and runs only the requested selection. It rejects
unknown/empty selections and has no implicit all. Existing Qt Creator caches are
not repurposed. --config Release requires a separate directory.

Direct CMake/CTest also works: build a selected executable or ray_tests_<group>,
then ctest -R '^name$' or -L group --output-on-failure. Groups include cmake, layout,
generators, navigation, transport, weather, widgets, ais, signalk and settings.
Linux has ray_tests_socketcan. Windows timing/rollover use explicit target names
and the timing label. Python tests have no C++ build prerequisite except settings
isolation, whose REQUIRED_FILES lists both settings_probe executables.

Helpers: RayNmeaSimulator-isolated, create_test_layout, network_can_peer,
settings_probe_isolated and settings_probe_production; route_acceptance_fixture
is Windows-only because its existing private UI uses volatile HKCU isolation.
Build explicitly using cmake-dev.py build --target <names>. Interactive isolated
launch uses run --target RayNmeaSimulator-isolated, optionally --smoke for the
existing real-entry startup check. Route fixture run supports --smoke and a thin
PowerShell wrapper. Helpers are not all automatically registered/executed as tests.

run-cmake-test.py is the existing common execution boundary: fresh private profiles,
working directories and evidence, test credential namespaces, SDK runtime paths,
curated example routes, bounded runs, inherited live/replay opt-in removal and
owned localhost virtual bridge/key cleanup for relevant suites. QtTest XML,
output.txt/result.json and independent seven-sentence validation are preserved.
Manual helper sessions are bounded to one hour; tests retain their 150-second
launcher bound. Test Results and raw output distinguish failure/skip/Not Run.
Do not run CTest targets via a normal production-profile application run button.

## Linux credential integration

Credential-dependent registrations carry the `credentials` label. CTest passes
`--credentials` to the common launcher on Linux only. This starts an owned private
D-Bus and real GNOME Keyring provider, with fresh HOME/XDG data/runtime/control
directories. The bus has no service activation directories. Its bus/provider PIDs
are verified before tests; a synthetic create/read/replace/remove probe must pass.
No desktop bus, existing keyring or real credential is used. Other Linux tests
receive an unavailable private bus address rather than inherited desktop access.
Windows retains its existing test-namespaced Credential Manager path.

Linux prerequisites: `dbus-daemon`, `dbus-send`, `gnome-keyring-daemon`, `secret-tool`
(distribution D-Bus, GNOME Keyring and libsecret tools). Missing tools or a provider
failure fail the selected test with an explicit diagnostic, never a desktop
fallback or an automatic package install. These are test prerequisites, not a new
production service. Install only through the developer's approved package workflow.

```text
python3 scripts/cmake-dev.py test --build-root build/credential-tests --qt-root <Qt-prefix> --suites sending_controls_tests settings_units_tests credential_isolation_tests
```

For a specific QtTest function use `run-cmake-test.py --credentials` on Linux with
the selected executable/function after `--`; retain all normal isolation arguments.
The fixture stops only its owned processes and deletes its fresh keyring on exit,
failure, timeout or handled termination. `credential-session.json`/`result.json`
retain provider, synthetic-probe and cleanup results, never passwords or stored
values. SIGKILL/power loss cannot run cleanup; no persistent daemon is installed.
The Linux `credential_isolation_tests` registration covers actual provider CRUD,
environment isolation, missing prerequisites, startup failure and launcher cleanup.
The existing fake `secret-tool` pipe-contract unit test remains separately named;
it is not evidence of real Secret Service integration.

Template authoring fixtures are now CMake-only and preserve source CRUD, backup,
safety/path checks, resource rename/delete/empty-catalog rebuild and Debug/Release
policy coverage. Production/test objects and test credentials are separately
compiled. The route fixture's memory backend is not replaced by an OS backend.

Historical suite descriptions and old build instructions remain verbatim in
[archived suite notes](https://github.com/RFStateSide/RayNmeaSimulator-Archive/blob/c8f2b5c822050ac875368a02b517e38a14a108b4/docs/history/tests-README-before-qmake-retirement.md).
Read them as historical context, not commands for the current checkout. Do not
remove their test source/fixtures or claim an unexecuted suite passed. Optional
archived/live/provider/platform checks retain their separate opt-in boundaries.
Current native IDE and focused qualification evidence is in README/platform guides;
physical CAN delivery is not established by synthetic or offscreen checks.

Historical evidence links target the private RayNmeaSimulator-Archive repository;
they are not required to follow the current build and test instructions.

Optional different-computer and live-network fixtures have no default remote host.
In a separately authorized test setup, supply `RAY_MULTI_COMPUTER_HOST` or
`RAY_NETWORK_LIVE_HOST` together with the corresponding existing explicit opt-in.
Ordinary isolated CTest launches remove these opt-ins; they never select an owner
network endpoint from the source tree.
