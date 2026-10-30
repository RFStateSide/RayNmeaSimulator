# Debian 12 ARM64 deployment baseline

For the current wrapper and exact output paths, see [deployment/linux/README-Debian12-ARM64.md](../deployment/linux/README-Debian12-ARM64.md).

M2.52's native Debian 13 ARM64 candidate is not Debian 12 compatible. Lowering
only the application's compiler flags or bundling a newer libc is not the
accepted approach. Build both Qt and the application against Debian 12; retain
the host and target OS libraries unchanged. No packaging or system installation
is triggered by these notes.

The accepted private replacement was built natively on ARM64 in a rootless
Debian 12 build filesystem using Bubblewrap/fakeroot; runtime qualification used
an isolated network namespace. Its base
was `library/debian:bookworm-slim`, ARM64 OCI manifest
`sha256:7ccb8b5b60c7654f5b795d94ee7d751cd0811500803f0b09cefcd555e755a55a`,
layer `sha256:f791e47cf52abf7b364878f2141547e3781072e437f599f6632849d88fca8ca4`.
The retained engineering evidence records resolved package versions; the moving
Debian package repositories are not a claim of bit-for-bit rebuildability.

Use a fresh private build filesystem, matching Qt 6.10.2 sources, and the accepted
marine dependency revisions/complete patch from `dependencies/pins.json`.
Keep the owner's home, configuration and hardware devices outside the sandbox;
mount source/dependencies read-only and use private build/output paths. Install
build prerequisites only within that disposable filesystem. Never upgrade a
test machine's OS or replace its libc/libstdc++ to fit a candidate.

Qt runtime recipe: shared Release, `-nomake examples -nomake tests`, modules
`qtbase,qtserialport,qtwebsockets,qtpositioning,qtcharts,qtsvg,qtimageformats,qtwayland`.
Build with Debian 12 GCC 12, `-O2 -DNDEBUG`, an isolated Qt prefix and matching
license/source tree. Generate SPDX JSON inventories (the engineering build used
`spdx-tools==0.8.3`). Explicitly enable `FEATURE_xcb`, `FEATURE_xcb_xlib`,
`FEATURE_xcb_glx_plugin` and `FEATURE_xcb_egl_plugin`; disable Wayland server and
compositor Quick features and unnecessary QML/Multimedia modules. The initial
missing-X11 attempt is preserved as a failed engineering attempt. Require both
XCB and Wayland client platform plugins in the final private runtime.

Within that same baseline, configure the application with a fresh build directory:

```text
cmake -S <source> -B <private-build> -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG" -DRAYNMEA_PACKAGE_VARIANT=Debian12 -DBUILD_TESTING=OFF -DCMAKE_PREFIX_PATH=<debian12-qt-prefix> -DDEPS_ROOT=<verified-stage> -DRAYNMEA_QT_SOURCES=<matching-qt-sources>
cmake --build <private-build> --target Deploy
```

This uses the shared `scripts/package.py` workflow. `debian_package()` handles
merged-/usr ownership aliases without changing system paths. The installer has
an executable Python 3 shebang; `install.sh` is the normal entry point. Only
actually used Qt components are required by the application CMake configuration.
The packager's measured ABI requirements and loader audit, not the archive name,
determine compatibility. The original engineering archive was copied under the
explicit `-Debian12` filename; the current Debian12 variant now adds that suffix automatically after its
explicit environment/ABI gates pass. Do not overwrite a previous candidate or its failure evidence.

Before selecting a new candidate, rehash its full manifest, audit every ELF's
architecture/RUNPATH/closure with the development Qt hidden, and compare GLIBC,
GLIBCXX/CXXABI and external library requirements with the intended target.
Exercise the installer lifecycle and production-profile tests in disposable homes.
Check X11 and offscreen startup, Qt library origins and the packaged TLS plugin
without making a network connection or enabling NMEA transmission. Qt/ICU/SDL2
are bundled; libc, libstdc++, desktop/graphics infrastructure and OpenSSL 3 remain
system prerequisites for the accepted Debian 12 candidate. No installer changes
those libraries. Publication and corresponding-source gates remain open as
described in [deployment.md](deployment.md).

The original recipe, OCI/package/tool locks, startup and TLS probes, loader audit,
archive manifest, failed attempts and delivery hashes remain private ignored
engineering evidence under `build/m252/debian12-arm64/`. M2.52C records the
owner-reported replacement installation acceptance without regenerating artifacts.
