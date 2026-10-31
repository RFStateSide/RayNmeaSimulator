# Build installation packages

**Qt Creator: select MinGW Release → refresh CMake → select the `Deploy` build
target → build.** No extra deployment cache option or script editing is needed.
See the complete [Windows instructions](windows/README.md),
[native Linux instructions](linux/README.md), and
[Debian 12 ARM64 instructions](linux/README-Debian12-ARM64.md).

Deploy builds platform-specific installation packages in fresh internal generations.
Validate each artifact on the supported target environment, retain its matching
source and notices, and verify hashes before distribution. See
[source delivery](../docs/source-delivery.md) and the platform instructions below.

## Where the files go

All paths below are relative to the **selected Qt Creator Release build directory**
(shown in Projects → Build Settings → Build directory), not the source directory:

```text
deployment/
  windows/RayNmeaSimulator.iss    # generated Windows installer configuration
  build/                        # private curated Release compilation
  deploy.log                    # latest configure/build/package log
deploy/
  RayNmeaSimulator/              # complete staged application, runtimes, notices
  installer/                    # completed installer or Linux archive + SHA-256
    build-metadata.json          # source/host/compiler/ABI information
  deployment-result.json        # latest attempt: running, failed or succeeded
  .runs/<unique-generation>/     # private intermediates and retained previous output
```

Windows installer: `deploy/installer/RayNmeaSimulator-0.1.0-Windows-x64.zip`.
Native Linux: `deploy/installer/RayNmeaSimulator-0.1.0-Linux-x86_64.tar.gz` or
`RayNmeaSimulator-0.1.0-Linux-aarch64.tar.gz`.
Debian 12 ARM64: `deploy/installer/RayNmeaSimulator-0.1.0-Linux-aarch64-Debian12.tar.gz`.
The version follows the application version; paths stay predictable.

Deploy builds a separate Release binary with the six owner-approved v0.1.0
Templates and the compiled approved Default Layout. It does not rebuild/replace
the ordinary development binary or include other owner-authored JSON files.
The build-time content probe verifies the same resource objects linked into the
application against their source bytes before packaging. Its inventory and
hashes are recorded in `build-metadata.json`; the probe is not installed.
See [the bundled content inventory](../resources/BUNDLED-CONTENT.md).

## Prerequisites

- CMake >=3.24, Ninja, Python >=3.11, the accepted native compiler/Qt kit and a
  verified dependency stage from [dependency setup](../docs/DEPENDENCIES.txt).
- Matching Qt source trees and SDK SPDX inventories for complete runtime notices.
  The installed Qt Sources component beside the selected Qt version is the default.
  `RAYNMEA_QT_SOURCES` can select another matching source tree.
- Windows: Qt 6.10.3 MinGW 64-bit/GCC 13.1, staged SDL2 2.30.9 and Inno Setup 6.
  `RAYNMEA_ISCC` selects a non-default Inno Setup compiler location.
- Linux: matching native Qt >=6.10, SDL2 development files via pkg-config,
  binutils (`readelf`, `objdump`, `strip`), `patchelf`, `ldd` and `dpkg-query`.
  Linux workflows currently target Debian/Ubuntu package ownership conventions.
- Deploy never installs OS prerequisites or changes services. Missing inputs fail.
  A matching pinned ICU notice may be fetched on first native packaging when
  required; a verified build-local cache is subsequently reused.

## Inspect and verify

Read Compile Output: require a successful exit and the final staged path,
installer/archive path and SHA-256 summary. `deploy/deployment-result.json` must
say `succeeded`. Inspect the staged `manifest.json`, `build-metadata.json`,
`licenses/`, `LICENSE`, `THIRD_PARTY_NOTICES.md` and `DISTRIBUTION.txt`. Linux
`system-runtime.json` and the installer folder's loader audit describe prerequisites.
The manifest enumerates staged files (the manifest itself is intentionally excluded).

PowerShell:

```powershell
Get-FileHash "<Release-build>/deploy/installer/RayNmeaSimulator-0.1.0-Windows-x64.zip" -Algorithm SHA256
Get-Content "<Release-build>/deploy/installer/RayNmeaSimulator-0.1.0-Windows-x64.zip.sha256"
```

Linux, from `deploy/installer/`:

```sh
sha256sum -c RayNmeaSimulator-0.1.0-Linux-x86_64.tar.gz.sha256
tar -tzf RayNmeaSimulator-0.1.0-Linux-x86_64.tar.gz | less
```

Substitute the ARM64/Debian12 filename when appropriate. Hash matching proves
file consistency, not trust, clean-machine acceptance or license compliance.

## Failed or repeated Deploy

The adapter invalidates the current successful paths **before compiling**. It
moves previous managed stage/installer/configuration into the new generation's
`previous/` folder, preserving evidence and even local changes to a staged tree.
On failure there is no current installer/stage; status is `failed` and logs/
intermediates remain. Another successful Deploy repopulates the stable paths.
Never treat a previous generation, a prior `succeeded` record after a configure
failure that prevented Deploy from starting, or a nonzero build exit as a new
successful package. Concurrent Deploy runs are refused.

Unmanaged or redirected output paths are refused rather than deleted. Preserve
them and choose a fresh Release shadow-build directory; do not use an in-source
build. There is no automatic cleanup of retained generations. Copy/retain needed
engineering evidence before any owner-directed cleanup. Build/deploy outputs are
ignored by Git; the source `deployment/` directory is maintained source.

## Test before distribution

Use disposable installation prefixes/profiles and separate compatible machines.
Test main/About/Help, runtime resources/themes, safe local Viewer/Generate use,
normal exit, upgrade and uninstall data preservation. Do not enable real marine
transmission for packaging qualification. Installer behavior is described in
[deployment notes](../docs/deployment.md); production probes can use
`--no-nmea --isolated-profile <absolute-disposable-folder>`.

Run the build-independent workflow regressions with
`python -B tests/deployment_workflow_tests.py`; they are also registered as
`deployment_workflow_tests` in normal CTest builds. Run the existing Linux installer
tests against a freshly extracted package in a disposable environment.

There is no signing, upload, GitHub Release, tag or publication step in Deploy.
Supply complete matching source and notices through the [source delivery workflow](../docs/source-delivery.md).
Release authorization and any outstanding compliance decisions are maintained in
PUBLIC_RELEASE_AUDIT.md.

## BrickSuite comparison

The reference is the actual Windows workflow at `RFStateSide/BrickSuite`, local
revision `1e2b8f870200b634806ac37dc3629049162a7915`: generated configuration under
`deployment/windows`, staging under `deploy/<application>`, installer under
`deploy/installer`, matching Qt/notices and a final path summary. Its Windows
batch entry point and Linux Qt Creator custom target provided the organizational
reference. RayNmeaSimulator adds a common Windows/Linux `Deploy` target, an
isolated curated compilation and explicit current-attempt status/history. It
retains its own installer identity, data-preservation policy and ABI baselines.

## Update information

Explicit Deploy also writes `deploy/installer/update-package.json`, a measured
package receipt for future official release metadata. Normal builds do not generate
or publish update information. See [release metadata](update/README.md) for the
separate local assembly and owner-authorized publication procedure.
