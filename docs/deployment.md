# Private installer and deployment workflow

RayNmeaSimulator 0.1.0 deployment adapts RF StateSide BrickSuite's current
Windows Inno Setup and Linux relocatable runtime/managed-prefix architecture.
Reference: `1e2b8f870200b634806ac37dc3629049162a7915`. Internal reuse is
owner-authorized; BrickSuite is unchanged. Public release remains **NO-GO**.

For current practical instructions, start with [deployment/README.md](../deployment/README.md), then the [Windows](../deployment/windows/README.md) or [Linux/ARM64](../deployment/linux/README.md) guide. The following engineering details retain M2.52 acceptance history.

Use a Release shadow build, the accepted native Qt kit, and verified dependencies.
Do not repurpose an owner Qt Creator build or install over the owner's application.

```text
python scripts/dependencies.py acquire --root <new-private-stage> --with-sdl2
cmake --preset deployment-release -DCMAKE_PREFIX_PATH=<matching-Qt-prefix> -DDEPS_ROOT=<new-private-stage>
cmake --build --preset deployment-release
```

Use `python3` on Linux and omit `--with-sdl2` there; SDL2 development files are
selected through native pkg-config. On Windows put the accepted Qt 6.10.3 MinGW
13.1 compiler and Ninja on PATH. `RAYNMEA_DEPS_ROOT` supplies the default dependency
stage for a **new** CMake cache; explicit/cached `DEPS_ROOT` takes precedence.
Without either override, the portable default is `<source>/.dependencies`.
Never embed a historical Ubuntu checkout name or an owner's home in shared presets.

The Windows clone-local NMEA0183 mismatch was an incomplete overlay: printf was
patched, but the approved RMC changes were absent. The approved revision and patch
SHA-256 in `dependencies/pins.json` remain unchanged. Fresh acquisition reproduces
the full accepted overlay. Preserve an incomplete stage for inspection; use a new
stage, verify it, then select it explicitly. Do not change the approved checksum,
relax verification, or silently fall back to a historical dependency directory.

In Qt Creator select the accepted native Release configuration, refresh CMake,
then select **Deploy** as its CMake Build Steps target. No deployment-mode override
is required: the adapter creates a separate curated Release build, without changing
the development executable or its options. The CLI equivalent is
`cmake --build <Release-build> --target Deploy`. Debug configurations are rejected.
Deployment embeds the six owner-approved Templates and Default Layout, all 49 Help topics,
illustrations, six themes and the approved icon. Extra owner-authored JSON is excluded.

`RAYNMEA_QT_SOURCES` selects the matching Qt source tree for notices (default:
SDK sibling `Src`). `RAYNMEA_ISCC` selects Inno Setup on Windows. Linux packaging
requires binutils, patchelf, ldd, dpkg-query and Python >=3.11; these are build
prerequisites. No packaging target installs OS packages. Missing inputs fail.
The ICU 73.2 notice is downloaded only when required, pinned to the same reviewed
URL/SHA-256 as BrickSuite; its verified build-local cache is reused.

Each invocation uses a fresh internal generation, then presents predictable outputs
under `<Release-build>/deploy/RayNmeaSimulator` and `deploy/installer`. The generated
Windows Inno configuration is under `<Release-build>/deployment/windows`. Current
success is invalidated before compiling; failures retain diagnostics/previous
outputs internally and never advertise an old installer as the new result. Check
exit status and `deploy/deployment-result.json`. See the central guide for details.
This is repeatable construction, not bit-identical Inno/tar output. Packages retain
per-file manifests, build/runtime metadata, notices, SDK SBOMs and distribution status.

## Windows acceptance

The x64 installer identifies RF StateSide LLC, uses the approved icon, registers
Add/Remove Programs and Start Menu entries, offers an unchecked desktop shortcut,
and defaults to `%LOCALAPPDATA%/Programs/RayNmeaSimulator`. It requires no admin.
Its new, stable AppId is `{E3048447-B053-4C82-8A90-45C05B214BC7}`. Upgrades retain
the chosen directory. Restart Manager handles applications in use through its
normal installer prompts; there is no forced-close option or post-install launch.
User data/credentials are outside the installer file list and uninstall has no
user-data deletion rule. Back up user data before any owner-approved upgrade.

The owner reports successful private installation on a clean Windows x64 machine
for M2.52C. Earlier disposable installation under `build/m252` was development-machine
qualification only. For another disposable test, first verify that the RayNmeaSimulator Start Menu group and
installer registration are absent; close/uninstall the test installation before
accepting an owner installation. Unsigned private candidates
are not public releases. Windows 10/11 x64 is the intended OS family; minimum
Windows build and broader graphics/TLS coverage have not been exhaustively qualified.

## Linux acceptance

Extract the native archive into a private directory. Run `./install.sh` for a
per-user installation at `~/.local/opt/RayNmeaSimulator`; `--prefix <absolute-dir>`
selects a disposable alternative. The script creates an application-menu entry
under `$XDG_DATA_HOME/applications` (default `~/.local/share/applications`). It
requires Python >=3.11 and compatible system runtime libraries, not Qt Creator,
the Qt SDK, a compiler or sudo. Launch the installed `RayNmeaSimulator` wrapper.

Upgrade by extracting the new archive elsewhere, closing the application and
running its `install.sh` with the same prefix/XDG environment. Hashes are checked
before copying; the replacement prefix is prepared alongside the old one and
renamed with rollback. Modified program files, extra files, redirected prefixes,
changed desktop registrations and running executables are preserved/refused.
Run installed `uninstall.sh --prefix <same-prefix>` to remove managed program
files and its launcher. Settings, routes, logs, QSS, credentials and keyrings
remain untouched. Do not save user files inside the program prefix.

The native candidates initially target Ubuntu **26.04 x86_64 / Qt 6.10.3** and
Debian **13 ARM64 / Qt 6.10.2**. They are not BrickSuite Ubuntu 22.04 baseline
artifacts. `build-metadata.json` records the exact build OS and measured
GLIBC/GLIBCXX/CXXABI requirements; `system-runtime.json` records external system
libraries/package versions. Qt, ICU where used, SDL2 and selected private runtime
libraries are bundled. libc, graphics drivers, desktop/session infrastructure,
libstdc++/libgcc and system libraries remain OS prerequisites. Inspect the runtime
manifest before choosing another distro. The installer checks architecture,
glibc and loader resolution. Native X11/Wayland desktop behavior is separate
from offscreen startup on development hosts.

### M2.52C acceptance and Debian 12 ARM64 distinction

The owner reports successful private installation tests on a clean Windows x64
machine, a compatible Ubuntu x86_64 desktop, and clean **Rpi6Boat** using
`RayNmeaSimulator-0.1.0-Linux-aarch64-Debian12.tar.gz`. These are owner-reported
acceptance results, separate from the recorded engineering probes.

The **original** native Debian 13 ARM64 candidate failed compatibility acceptance
on the Boat test machine: its installer required glibc 2.38, while the Debian 12
machine provides 2.36. That failure remains historical qualification evidence;
it has not been changed to PASS. The replacement archive has SHA-256
`2e358ec2bd35c2574c42eb2c34e8735d46f611b943a58401fb7f6bec9120e7ec`.
It rebuilt Qt 6.10.2 and the application inside an isolated native ARM64 Debian 12
filesystem. Its measured requirements are GLIBC 2.35, GLIBCXX 3.4.30 and CXXABI
1.3.13. Its 51 ELF files and 513 manifest entries were checked, and both XCB/X11
and Wayland client plugins were included. No Boat OS/system-library replacement
was required. See [Debian 12 build notes](deployment-debian12-arm64.md).

The accepted source includes the Python installer shebang, merged-/usr package
ownership lookup for runtime manifests/notices, and removal of unused Qt Location
and Multimedia build/link requirements. A native Debian 13 Deploy run still
produces a Debian 13 candidate; it does not automatically lower its ABI baseline.
Use the matching Debian 12 application **and Qt runtime** build for that baseline.
Existing private candidates remain historical evidence and are not rebuilt just
to update their embedded acceptance metadata or the later Help labels.

### Approved bundled Templates and Default Layout

M2.54 selects exactly Default, Environment / Weather, Navigation / Autopilot,
Single Engine, Synthetic AIS Demonstration and Twin Engine for both development
and deployment. See [the source/resource inventory](../resources/BUNDLED-CONTENT.md).
`resources/templates/templates.cmake` is explicit; other saved source files and
user Templates are preserved but never implicitly added to compiled snapshots.
`catalog.txt` remains the rcc dependency marker. The build-only content probe
checks the actual application resource objects and source bytes before packaging.

The approved Default is `resources/defaults/defaultLayout.json`, compiled by
CMake as `:/defaults/defaultLayout.json`. It replaces the application resource's
historical test fixture without replacing any user Default. `AppPaths::defaultLayout`
continues to prefer an existing user current/legacy Default. Loading preserves
serialized states without authorizing external NMEA Sending. The historical
fixture remains in isolated unit scenarios; compiled-content regressions use
actual application resources. Final Help/release polish and publication remain
separate owner decisions.

The Ubuntu accepted checkout retains its historical
`RayNmeaSimulator-M2.30-Acceptance` name. Relocation is unnecessary for packaging.
If desired later, obtain owner approval, create a new checkout at the accepted
revision, provision clone-local dependencies, select a new Creator configuration,
and qualify it before retiring anything. Preserve existing kits, caches and builds;
do not restore/reset QtCreator.db or rename the current checkout.

## Safe production-binary qualification

```text
<installed-executable> --no-nmea --isolated-profile <absolute-disposable-directory>
```

This uses the real Release binary with INI settings and data beneath that directory,
disables native-settings fallback, and blocks credential-store operations. Missing
`--no-nmea`, relative/home/symlink profiles and redirected settings files fail closed.
Normal launches keep the established `RFStateSide` / `RayNmeaSimulator` identity.
Do not interpret this opt-in profile as a new production settings migration.

Before owner acceptance check installed main/About/icon, all six themes and an
external QSS with assets, all offline Help content/images, local Viewer/Generate,
synthetic Layouts/Templates, save/reload, voluntary links through explicit action,
clean exit, upgrade preservation and uninstall preservation. Never enable real
hardware TX as part of this assignment. The engineering report separates completed
checks from remaining clean-machine/native-desktop/hardware acceptance.

## Distribution obligations

Qt Charts is GPL-3.0-only/commercial; this candidate uses the open-source terms.
The matching SDK SBOM and source headers confirm that it is not LGPL. Other Qt
modules and third-party code retain their respective terms. AGPLv3/GPLv3 combined
distribution is addressed by section 13; corresponding-source and other license
conditions remain applicable. Full source/build/installation information must be
delivered through a compliant method before external distribution. License summaries
or private repository links alone do not satisfy that requirement.

See `DISTRIBUTION.txt`, the package manifest and the milestone report. Historical
source/artwork/publication review, recipient source access/delivery, exact transitive
source availability and owner approval remain publication gates. No package target
publishes, uploads, creates tags or changes repository visibility.
