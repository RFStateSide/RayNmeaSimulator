# Debian 12-compatible ARM64 package

Use a **Linux ARM64 development host**, an isolated Debian 12 userspace, and Qt
**6.10.2 built against that userspace**. A Qt SDK/application built on Debian 13
does not become Debian 12 compatible by renaming its archive or changing one
compiler flag. Windows/x86_64 cross-building is not provided by this workflow.
Use the matching baseline package; do not replace a target system's glibc or Qt
to make a newer-host build run.

## Qualified inputs (one-time preparation)

The ARM64 development baseline contains:

```text
<baseline>/rootfs/                 Debian 12 ARM64 userspace + build tools
<baseline>/work/qt/                Qt 6.10.2 built inside that baseline
<matching-Qt-sources>/             complete matching Qt notice/source tree
<verified-marine-dependencies>/    accepted pins and complete NMEA0183 patch
<CMake-tools>/bin/cmake            CMake executable usable inside Debian 12
```

`<baseline>` is the previously qualified ignored `build/m252/debian12-arm64`
workspace, or a separately prepared equivalent. Preserve the old artifacts and
evidence. Its locked OCI manifest, GCC 12 runtime recipe, Qt modules/features,
package/tool inventories and qualification evidence are described in
[the baseline notes](../../docs/deployment-debian12-arm64.md). No script here
silently downloads/rebuilds Qt or installs a baseline.

The host needs Python >=3.11 and Bubblewrap with working unprivileged namespaces.
The isolated baseline supplies GCC 12/build-essential, Git, Python >=3.11, Ninja,
pkg-config, SDL2 development files, binutils, patchelf, dpkg-query and the Qt
graphics/font/desktop development prerequisites from the qualified inventory.
If provisioning an equivalent private rootfs, create its empty mount directories
`src`, `deps`, `qt-src`, `tools/cmake`, `work`, `proc`, `dev`, `tmp`, `run` within
that private rootfs before use. Never create/alter these paths on the host root.
Full Qt Sources and matching SPDX JSON inventories are required for notices.
Retain XCB/X11 and Wayland client plugins; the original missing-X11 attempt is
failed historical evidence, not an accepted baseline.

## One-command package using the existing baseline

From the current source root on the ARM64 development host, substitute physical
absolute paths to those existing inputs and a fresh ignored output directory:

```sh
python3 deployment/linux/package_debian12.py \
  --rootfs /absolute/baseline/rootfs \
  --qt /absolute/baseline/work/qt \
  --qt-sources /absolute/Qt/6.10.2/Src \
  --deps /absolute/qualified-dependencies \
  --cmake-tools /absolute/Qt/Tools/CMake \
  --build "$PWD/build/debian12-package"
```

The wrapper mounts the source, baseline, Qt, CMake and dependencies **read-only**,
uses a private temporary home/minimal devices and disables networking. Only the
explicit output directory is writable. It configures Release, then invokes the
same `Deploy` adapter inside the baseline. It neither launches the application
nor accesses owner profiles, marine interfaces or services.

Qualify each archive against the ABI ceilings below, full manifest/ELF and permission
checks, disposable installer lifecycle tests and offscreen/X11 startup with the SDK
absent. Verify offline Help and normal exit without physical transmission.

For a shell already inside a prepared Debian 12 ARM64 environment, the equivalent is:

```sh
cmake -S /src -B /work/output -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS_RELEASE='-O2 -DNDEBUG' -DBUILD_TESTING=OFF \
  -DCMAKE_PREFIX_PATH=/work/qt -DDEPS_ROOT=/deps \
  -DRAYNMEA_QT_SOURCES=/qt-src -DRAYNMEA_PACKAGE_VARIANT=Debian12 &&
cmake --build /work/output --target Deploy
```

## Output and compatibility gates

On the host, all paths below are relative to the selected `--build` directory.

Build summaries and metadata use sandbox paths: `/src` is the source snapshot,
`/work/qt` is the read-only build SDK and `/work/output` maps to the host `--build`
directory. Retrieve packages from that host directory, not a host `/work/output`.
The runtime uses relative packaged library/plugin paths and does not require the
build SDK to exist.

```text
deploy/RayNmeaSimulator/                  staged application and private runtime
deploy/installer/RayNmeaSimulator-0.1.0-Linux-aarch64-Debian12.tar.gz
deploy/installer/RayNmeaSimulator-0.1.0-Linux-aarch64-Debian12.tar.gz.sha256
deploy/installer/build-metadata.json
deploy/installer/loader-audit.json
deploy/deployment-result.json
```

`Debian12` packaging requires ARM64, Debian 12 userspace and measured maxima of
**GLIBC 2.35 / GLIBCXX 3.4.30 / CXXABI 1.3.13**, matching the qualified replacement.
Missing measurements or a higher requirement fail packaging. The accepted target
provided glibc 2.36; no new package may silently raise the qualified ceiling.
Qt/ICU/SDL2 are private to the archive; libc/libstdc++, graphics/session libraries
and OpenSSL 3 remain compatible system prerequisites. Read `system-runtime.json`.
The guard is not a substitute for loader/platform-plugin and clean-machine tests.

Verify the sidecar with `sha256sum -c`, inspect the archive/manifest/notices and
test installation in a fresh extraction/disposable prefix before owner testing.
The original native ARM64 package's glibc 2.38 failure remains a failure. The
previous `-Debian12` replacement's successful installation is owner-reported;
new packages need new acceptance. No package publication or artifact upload occurs.
