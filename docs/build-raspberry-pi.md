# Native Raspberry Pi ARM64 CMake development

Rpi5_ssd: Debian13.3 ARM64, Qt6.10.2 gcc_arm64, GCC14.2, native SDL2,
CMake3.30.5 and Ninja1.12.1. Native CMake Debug/Release Qt Creator build/run
and GDB are owner accepted; 14 selected Debug CTest registrations passed.
Focused IDE parity/startup tests passed, including DebugTests persistence repair.
These results are not a full suite, deployment or physical CAN qualification.

Use [the CMake guide](build-cmake.md). Provide native ARM64 Qt modules/tools,
compiler, pkg-config and SDL2 development prerequisites. All selected Qt tools
(moc, uic, rcc and transitive QML tools) and libraries must execute/load on the
host ABI before application compilation. An installed folder is not SDK proof.

```sh
python3 scripts/dependencies.py acquire
python3 scripts/dependencies.py verify
cmake --preset dev-debug -DCMAKE_PREFIX_PATH="$QT_ROOT"
cmake --build --preset dev-debug --target RayNmeaSimulator --parallel 2
python3 scripts/cmake-dev.py test --build-root build/native-tests --qt-root "$QT_ROOT" --suites cmake_parity_tests cmake_startup_smoke --jobs 2
```

QT_ROOT is the matching gcc_arm64 prefix. Keep existing local roots/Qt Creator
configurations and separate Debug, Release and DebugTests directories. Select the
qualified native kit, not an unconfigured generic preset whose Qt prefix is empty.
DebugTests uses BUILD_TESTING=ON and explicit ray_tests_cmake build steps. CTest-only
discovery avoids duplicate QtTest execution. Clear filter/Deselect All/reselect on
reopen; refresh CTest or reopen if switching from application-only configuration
leaves CTest(none). Existing accepted settings need no migration for retirement.

SocketCAN source is selected only on Linux. Existing can0/SPI/HAT/startup services
are external prerequisites and are never configured or exercised by build tests.
Linux DESTDIR install staging follows the Ubuntu guide; runtime packages remain #18.

Rpi5_new is an OpenPlotter/OpenCPN/Signal K monitoring host, excluded from intended
RayNmeaSimulator build qualification. Its Debian12 SDK ABI incompatibility and
possible migration are deferred and are not a CMake/qmake retirement blocker.
Do not change that host, its SDK, monitoring services or CAN setup for this work.
Earlier ARM64 observations/limitations are preserved verbatim under history and
in the original M2.45E engineering report. Historical toolchain failures are not
current Rpi5_ssd failures and do not justify changing system libraries.
