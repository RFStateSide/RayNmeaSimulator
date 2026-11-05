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

## Upgrading an earlier development launcher

An earlier Qt Creator registration can use the same application-menu ID as the
archive installer. When there is no managed installation receipt, the installer
accepts replacement only if the existing entry unambiguously identifies
RayNmeaSimulator and directly references an existing executable owned by the user.
It never runs or changes that historical executable or its source tree.

The default `--desktop-replacement ask` prompts on an interactive terminal, with
**No** as the default. Approval creates a unique
`com.rfstateside.RayNmeaSimulator.desktop.<random-id>.bak` alongside the launcher,
preserving the original contents and Unix permission bits before replacement.
The backup ends in `.bak`, so it is not a second application-menu entry.
`--desktop-replacement preserve` declines replacement. Without an interactive
terminal, including `--non-interactive`, the default refuses the conflict; explicit
`--desktop-replacement replace` consents to backup and replacement. This option
does not bypass identity or filesystem safety checks. A declined or unsafe conflict
stops before dependency installation or application replacement.

Only the fixed user application-menu entry is replaced. Symlinks, hard links,
nonregular files, parent traversal, unresolved ownership, shared-write conditions
and extended access/default ACLs are refused. Normal Ubuntu private-user-group permissions
are supported. Shell wrappers, arguments, stale/missing executable targets and
complex or ambiguous unmanaged entries require manual owner review. Entries
already bound to an intact managed receipt retain normal upgrade handling;
modified managed registrations remain protected from overwrite.

The launcher is published by atomic rename from a new file in the checked
registration directory. If installation fails, the application transaction rolls
back and the original launcher bytes and permission bits are restored. If rollback
itself is blocked, the installer reports that failure and the preserved backup
path rather than claiming success. Backups are outside the application prefix and
survive subsequent upgrades and uninstall. Uninstall removes only the verified
managed launcher; it does not reactivate the old development launcher. To restore
one, inspect the reported backup and choose explicitly after uninstalling or
otherwise resolving the managed registration. No old executable, settings,
credentials, Templates, Layouts, unrelated launcher or GNOME configuration/cache is
removed by this migration.

### Published v0.1.0 launcher property edits

An intact official v0.1.0 receipt and launcher upgrade normally without a new
replacement prompt. A desktop-properties edit can change the launcher hash while
leaving its executable and icon correctly aimed at the managed program prefix.
That is a real launcher modification, not evidence of a broken receipt format.

For the original public v0.1.0 build and its accepted corrected distribution, the
installer can offer backup and replacement only after validating the entire old
program inventory, published clean-build identity, exact original launcher-recipe
hash in the receipt, and a narrow set of safe property edits. Primary and localized
executable/icon/type/name values must still identify the managed application;
unknown fields, actions, redirects or unrelated targets remain protected.

This path uses the same `--desktop-replacement ask|replace|preserve` consent policy.
Approval explicitly replaces launcher edits, including `Terminal=true`, with the
new defaults; it preserves the original bytes and Unix permissions in the reported
backup. The prompt explains that distinction. Decline, EOF, or noninteractive use
without explicit opt-in preserves both the launcher and old application. Changed
managed launchers still cannot be uninstalled through this exception. More extensive
edits, unknown build provenance, changed program files and unsafe filesystem state
continue to require manual owner review. No old-format receipt is rewritten before
successful application/launcher publication; normal rollback restores it exactly.

## Target runtime checks

The Linux installer verifies the shipped XCB/SVG/TLS plugin closure as well as the
executable; `libxcb-cursor0` is an X11 runtime requirement on all three Linux builds.
On supported Debian/Ubuntu/Mint systems missing runtime packages can be installed
only after explicit consent. Provisioned hosts still install per-user without sudo.
Use `--dependencies check` to refuse system-package installation and
`--non-interactive` to suppress all prompts;
`--dependencies install --non-interactive` explicitly opts into missing packages
with noninteractive sudo authorization. No automatic OS upgrade or system Qt/glibc
replacement is performed. See [packaging corrections and full source](../../docs/packaging-corrections.md).
