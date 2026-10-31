# Installing and building RayNmeaSimulator

Use the platform-specific installer/archive from the
[official releases](https://github.com/RFStateSide/RayNmeaSimulator/releases).
Verify its SHA-256 and read its system requirements and release notes. Packages
include Qt runtime files; running the application does not require Qt Creator or
an SDK. Choose the correct architecture and Linux baseline. macOS binaries are
not a supported v0.1.0 distribution.

## Windows x64

Close RayNmeaSimulator before upgrading. Run the x64 installer; it installs per
user under `%LOCALAPPDATA%/Programs/RayNmeaSimulator`, retains a chosen install
directory, registers an application-menu entry and offers an optional desktop
shortcut. Uninstall removes program files and registrations, not user data.
Keep bundled DLLs/plugins together. See [Windows details](../deployment/windows/README.md).

## Linux x86_64 and ARM64

Verify the sidecar checksum, extract the archive into a new directory and run
`./install.sh`. The default program prefix is `~/.local/opt/RayNmeaSimulator`;
`--prefix <absolute-directory>` selects another location. Installation requires
Python >=3.11 and compatible system libraries, not sudo or a developer SDK.
Launch the installed `RayNmeaSimulator` wrapper. Linux desktop libraries and the
OpenSSL 3 TLS runtime are system prerequisites; the exact recorded external baseline
is in `system-runtime.json`. A package is not universally portable across Linux.

For upgrade, close the app and run the newly extracted package's installer with
the same prefix/XDG environment. A prepared replacement and rollback protect the
managed program tree. Changed/extra program files, redirected prefixes, modified
launchers and running applications are refused rather than overwritten. Keep user
files outside the program prefix. `uninstall.sh --prefix <same-prefix>` removes
managed program files and its launcher; user settings, routes, Templates, Layouts,
custom themes, credentials and logs remain separate. Back up important user files.

Debian12 ARM64 uses the explicitly named `Linux-aarch64-Debian12.tar.gz` package.
A newer-host ARM64 package can require newer glibc and is not interchangeable.
Do not replace target system libraries to force an incompatible package to run.
See [native Linux](../deployment/linux/README.md) and
[Debian12 ARM64](../deployment/linux/README-Debian12-ARM64.md).

## Building packages

Use the accepted native Qt/compiler kit, verified dependencies, a fresh Release
shadow-build directory and the [Deploy workflow](../deployment/README.md). The
adapter creates a separate staged Release build and leaves development binaries
and options independent. Outputs are `deploy/RayNmeaSimulator` and `deploy/installer`
relative to that selected build directory. Only the six approved bundled Templates,
compiled Default Layout, 52 Help topics, images/themes and application icon are used.
Inspect `manifest.json`, `redistribution.json`, `source-snapshot.json`, notices,
version/architecture/ABI metadata and `deployment-result.json` before distribution.
Failed attempts retain evidence; never use a prior artifact as a successful new build.

## Distribution obligations

Supply matching source materials, licenses and installation/build information
beside each binary package. Follow [source delivery](source-delivery.md) and
[update metadata](../deployment/update/README.md); validate the exact artifacts
and SHA-256 receipts, not just a license summary. GPL/AGPL and dependency terms
remain applicable. The AS-IS designation does not waive those terms or imply
navigation/safety certification. Test the supported target environments without
physical NMEA transmission; retain documented platform/transport limitations.
