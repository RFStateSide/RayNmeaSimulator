# Native Ubuntu x86_64 and Linux ARM64 packages

These commands build a **native-host package**. Current qualified native
development environments are Ubuntu 26.04 x86_64 / Qt 6.10.3 and Debian 13 ARM64 /
Qt 6.10.2. Qt Creator, a compiler and an SDK are not required on the target
machine, but compatible system libraries and Python >=3.11 for installation are.
Measured requirements are recorded in each package; newer-host builds do not
automatically support older distributions. For Debian 12 ARM64 use the
[separate baseline workflow](README-Debian12-ARM64.md).

## Prerequisites

Native C++ compiler, CMake >=3.24, Ninja, Python >=3.11, matching native Qt >=6.10
and complete Qt Sources/SPDX inventories, a verified marine dependency stage,
pkg-config and SDL2 development files. Packaging additionally needs binutils,
patchelf, ldd (libc-bin) and dpkg-query. Native Debian/Ubuntu package metadata is
used for system-library versions and notices. No Deploy command installs packages,
uses sudo, changes services or upgrades the OS. Install missing build prerequisites
separately under owner control; do not alter a clean target machine for an incompatible package.

## Build

From the source root, set the two paths to existing matching inputs:

```sh
QT_PREFIX="$HOME/Qt/6.10.3/gcc_64"       # Ubuntu x86_64
# QT_PREFIX="$HOME/Qt/6.10.2/gcc_arm64" # native Debian 13 ARM64
DEPS_ROOT="$PWD/.dependencies"          # must pass dependencies.py verify
export PATH="$HOME/Qt/Tools/CMake/bin:$HOME/Qt/Tools/Ninja:$PATH"
cmake --preset dev-release -DCMAKE_PREFIX_PATH="$QT_PREFIX" -DDEPS_ROOT="$DEPS_ROOT" &&
cmake --build build/cmake/dev-release --target Deploy
```

If `patchelf` is installed in a local tools prefix, add its `bin` directory to
`PATH` before configuring CMake. The Deploy build uses that resolved tool path;
it does not install tools or modify system libraries.

If Qt Sources are not the SDK's sibling `Src`, add
`-DRAYNMEA_QT_SOURCES=/absolute/matching/qt-source-tree` to configure. Compiler,
SDK, dependencies and notices must all match the chosen native environment.
An existing Release build can simply run `cmake --build <build> --target Deploy`.
In Qt Creator select native Release, refresh CMake and choose **Deploy** under
Build Steps → CMake Build → Details. There is no extra deployment mode to select.

## Retrieve and inspect

For the command above, relative to `build/cmake/dev-release/`:

```text
deploy/RayNmeaSimulator/                 staged relocatable application
deploy/installer/RayNmeaSimulator-0.1.0-Linux-x86_64.tar.gz
deploy/installer/RayNmeaSimulator-0.1.0-Linux-x86_64.tar.gz.sha256
deploy/installer/build-metadata.json
deploy/installer/loader-audit.json
deploy/deployment-result.json
```

Native ARM64 uses `Linux-aarch64` in the filename. Read the package's measured
GLIBC/GLIBCXX/CXXABI requirements and `system-runtime.json`; inspect `manifest.json`
and `licenses/` in the stage. Qt/ICU/SDL2 and selected private libraries are bundled;
libc, libstdc++/libgcc, desktop/graphics infrastructure and other recorded system
prerequisites are not replaced. Require status `succeeded` and a zero Deploy exit.

```sh
cd build/cmake/dev-release/deploy/installer
sha256sum -c RayNmeaSimulator-0.1.0-Linux-x86_64.tar.gz.sha256
tar -tzf RayNmeaSimulator-0.1.0-Linux-x86_64.tar.gz
```

For disposable installation testing, extract into a new test directory and run
`./RayNmeaSimulator/install.sh --prefix /absolute/disposable/program-prefix` with
a disposable HOME/XDG_DATA_HOME. Run the installed `uninstall.sh` with the same
prefix/environment afterwards. Normal installation defaults to
`~/.local/opt/RayNmeaSimulator`; it does not require Qt to be installed system-wide.
Do not store owner data inside the managed program prefix. Modified/unmanaged
installations are preserved/refused. Settings, layouts, QSS, credentials, routes
and logs are outside installer ownership. See [the overview](../README.md) for
safe profile probes, failure handling, retained generations and release gates.
