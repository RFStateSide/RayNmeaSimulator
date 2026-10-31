# Source delivery and rebuilding

RayNmeaSimulator uses AGPL-3.0-or-later. Qt Charts and, when bundled, Qt Virtual
Keyboard use GPLv3 under the open-source option. Other components retain their
own notices and license choices. The AGPLv3/GPLv3 combination preserves each
license's requirements under section 13; it does not relicense upstream code.
See [application license](../LICENSE), [third-party notices](../THIRD_PARTY_NOTICES.md)
and the full package `licenses/` directory.

## Distribution method

For a network release, place each platform's complete matching **Source.tar.gz**,
its SHA-256 sidecar and per-platform `*-release-set.json` beside that platform's installer/archive
on the [official release page](https://github.com/RFStateSide/RayNmeaSimulator/releases).
Provide equivalent source access at no additional charge and retain that access
throughout the time the matching binaries are offered. Keep each version’s binary
and source assets together; do not remove source while continuing binary downloads. Point directly to those source assets next to the binary
downloads. A link to the application repository alone does not include matching Qt
or other dependencies. This method uses GPL/AGPL section 6(d), rather than relying
on an unimplemented written-source offer. For offline conveyance, supply matching
source on the same medium under section 6(a). Do not distribute a binary without
its complete matching source materials and notices.

The Source kit contains the application without private Git history, all forms,
resources, build/install/dependency scripts, pinned marine sources with their approved
patch state, matching Qt source archives, supplier SPDX/configuration records, bundled
runtime sources, source/index hashes and notices. Source archives retain their upstream
licenses. Compiler and OS tools not conveyed as part of the application are build
prerequisites, not application-owned code.

## Traceable build and package

1. Use a reviewed source revision and verified dependency stage. Normal builds record
   `source-snapshot.json` with Git revision, clean/modified state, every registered
   source file's SHA-256/mode and a canonical source-tree fingerprint.
2. Run the normal [Deploy workflow](../deployment/README.md). Packaging refuses source
   files changed since the build snapshot. The stage includes that receipt,
   `build-metadata.json`, `manifest.json`, `redistribution.json`, licenses and notices.
   Qt coverage is derived from actual deployed files and supplier SPDX records;
   runtime plugins are not assumed to match the requested CMake module list.
3. Acquire matching source inputs explicitly, in a physical build directory:

```sh
python -B deployment/acquire_sources.py \
  --stage build/release/deploy/RayNmeaSimulator \
  --qt /absolute/matching/Qt/prefix \
  --deps /absolute/verified/dependencies \
  --cache build/source-cache --output build/source-inputs.json
```

Qt release archives use official supplier SHA-256 files and matching source tags.
When a supplier identifies a different source commit, acquisition verifies that exact
Git object and its pinned submodules, includes their licenses and source, and records
the origin/tree identities; the generated `.tag` identifies that commit. Version-only
supplier records retain that evidence limitation explicitly. Additional Qt build
support sources are included when required. Marine snapshots preserve one pinned
upstream tree/commit and the exact approved patch; they have no private application
history or remote credentials. SDL2 uses its pinned upstream source on Windows.
Linux source inputs must also include the exact bundled distribution source packages,
with their `.dsc`, original archives and distribution patches, and matching ICU sources.
The acquisition catalog is explicit and hash-checked; missing material is an error.
If a supplier commit differs from the SDK's installed Sources component, extract the
acquired archives into a separate source directory, naming each module directory
`qtbase`, `qtdeclarative`, etc. Select that directory with `RAYNMEA_QT_SOURCES` and
rerun Deploy if notices differ. Preserve the installed SDK/Sources component.
Source sealing compares every deployed Qt module notice against the matching
source archive, allowing only LF/CRLF line-ending equivalence.

4. Assemble and verify a fresh source kit for each actual artifact:

```sh
python -B deployment/source_delivery.py assemble \
  --stage build/release/deploy/RayNmeaSimulator \
  --artifact build/release/deploy/installer/RayNmeaSimulator-0.1.0-Windows-x64.zip \
  --qt /absolute/matching/Qt/prefix --sources build/source-inputs.json \
  --output build/release-source
```

Use the Linux x86_64, native ARM64 or Debian12 ARM64 artifact for those platforms.
The sealer verifies the staged manifest, actual artifact/update receipt and source
snapshot, every supplied source checksum, Qt source identity and required components.
It refuses a modified application checkout for release sealing. `--allow-dirty` is
only for local qualification; the receipt records that state rather than presenting
it as an approved release revision. per-platform `*-release-set.json` binds installer, source archive,
platform, version, source fingerprint and manifest hashes. It is an integrity receipt,
not a legal certification or publication authorization. Assemble update metadata from
the same final package receipts using [the update workflow](../deployment/update/README.md).
Rebuild after accepting a source commit; do not relabel a prior working-tree package.

## Build from the kit

Extract the kit into a fresh directory, preserve `source-index.json` and verify its
hashes. The generated `application/.raynmea-source.json` allows build-time source
capture without the private application repository. Edited files are recorded as
modified; this does not prevent studying, changing or building the software.
Extract the Qt and runtime source archives under a separate source directory using
a safe archive extractor. Retain all licenses and distribution patches.

For the standard SDK reproduction, select the same Qt/compiler versions recorded in
`build-metadata.json` and supplier configuration/SPDX records. For an independently
built Qt, first configure/build/install qtbase, then the supplied modules with
`<Qt-prefix>/bin/qt-configure-module`; use the supplier feature/configuration records.
Build support includes qtshadertools/qtlanguageserver/qttools when Qt Quick is used.
Use a matching native Release compiler and prefix. Retain XCB/Wayland and TLS support
for the intended Linux desktop; do not claim a different OS/glibc baseline by changing
only compiler flags. These recipes reproduce the source/build relationship, not
bit-identical vendor SDK or installer output.

Extract each marine source archive to a dependency stage named `NMEA2000`, `NMEA0183`
and `NMEA2000_socketCAN` (rename the archive's `dependency` directory). The small Git
metadata contains only that public upstream commit/tree, so the unchanged mandatory
verifier can confirm the pins and approved patch without network access. Do not reset
the NMEA0183 working tree: its patch is already applied. Windows uses the pinned SDL2
MinGW SDK from `dependencies/sdl2-mingw.json`; the provided acquisition script obtains
it, and the full SDL2 source is included for rebuilding/modifying SDL. Linux uses its
matching SDL development environment. If producing a modified dependency/SDK, update
your derivative's lock/patch inputs consistently rather than disabling verification.

Build the application with the portable commands in [dependency setup](DEPENDENCIES.txt)
and [deployment](../deployment/README.md), selecting the kit's dependency stage as
`DEPS_ROOT` and your Qt prefix as `CMAKE_PREFIX_PATH`. Use a fresh build directory.
For Debian12 ARM64 use the [isolated baseline recipe](deployment-debian12-arm64.md);
its Qt and application must both target Debian12. Windows x64 and Linux x86_64 use
their native toolchains. Native ARM64 and Debian12 ARM64 source kits are separate
because their Qt/runtime inputs and ABI requirements differ.

Compatible modified dynamic libraries and modified application builds may be run
without a vendor activation key or update service. Use a new install prefix for a
modified build; the normal installer protects changed program files rather than
overwriting them. User Layouts, Templates and preferences remain separate. No device
signing keys, locked boot chain or hardware installation authorization is supplied or
required by these desktop packages. Any later appliance/User Product distribution
requires a separate review of installation-information obligations.

GCC libgcc/libstdc++ runtime DLLs retain GPL plus GCC Runtime Library Exception 3.1.
The ordinary GCC/MinGW open-source compilation recorded here uses that exception;
full notices are included, and those independent runtime DLLs are not relicensed.
Winpthreads/mingw-w64 have their own permissive notices; the matching full
MinGW-w64 source revision is included for the bundled winpthreads DLL. If modifying the runtime,
using a materially different compilation process, adding dependencies or distributing
hardware appliances, re-evaluate the applicable conditions and source scope.

## Installer and network operation

Windows packages use the unmodified Inno Setup engine. Its original embedded
copyrights and website addresses remain intact; the actual engine/compiler hashes,
version and full permissive license are recorded with each package. The source kit
also includes that upstream revision and its pinned Pascal Script submodule, with
their original notices. The application’s installation script is included with
the application source. The compiler itself is not installed with the application.

For modified AGPL versions offered for remote interaction, provide the prominent
matching-source offer required by AGPL section 13 through the service’s user-facing
access/connection instructions or interface. Do not inject notices into NMEA frames
or weaken Sending safeguards. This desktop package does not provide a hosted web
interface; assess any later hosted service separately.
