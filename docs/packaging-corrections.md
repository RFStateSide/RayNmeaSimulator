# Corrected v0.1.0 installation packages

The application remains version 0.1.0. The original annotated v0.1.0 tag remains
on `87cd0912b3e10777e610a82a78156484f66a2c12`; it must not be moved or recreated.
Corrected executables contain a repository-hosted updater and require a new normal
application/packaging source commit. They are not built entirely from that old tag.
Record the original tagged revision, corrected build revision/tree, each binary and
package SHA-256, matching complete Source kit, and final update-manifest SHA
separately. Uncommitted qualification snapshots are explicitly private/not ready
for publication. After acceptance, build final distributions from the clean approved
code commit, then commit the manifest derived from those verified final artifacts.

## Linux runtime prerequisites

Run `./install.sh` as your normal user from a newly extracted archive. Installation
verifies the package manifest, architecture, glibc compatibility and the actual
loader closure of the executable and shipped XCB, SVG and OpenSSL plugins. OpenSSL 3
is checked separately because Qt loads it dynamically. In particular, X11 startup
requires the distribution's `libxcb-cursor0`; checking the executable alone is
insufficient. The same check applies to x86_64, native ARM64 and Debian 12 ARM64.

Only missing, verified runtime SONAME providers are offered. On supported Debian,
Ubuntu and Linux Mint systems the installer shows package names and a manual
command, checks the apt plan and asks before using administrator privileges.
It rejects removal, installed-library upgrades and glibc/system-Qt replacement.
It never launches the application, manages services or replaces user data.
Only the system dependency command is elevated; the application install stays per-user.
No package-index refresh or distribution upgrade is performed automatically.

- `./install.sh --dependencies check`: check only; never install system packages.
- `./install.sh --non-interactive`: never prompt. Missing dependencies fail with
  actionable instructions; an already provisioned host still installs normally.
- `./install.sh --dependencies install --non-interactive`: explicit consent to
  install missing runtime packages. Unattended sudo uses `-n` and fails if credentials
  are unavailable. Provision privilege access separately; passwords are not read
  or stored by the installer. Interactive explicit consent may use normal sudo.

A declined, offline or failed dependency installation leaves the managed application
and desktop registration unchanged. A package-manager failure may leave some newly
installed system dependencies; review its output and retry. There is no automatic
system rollback or invented success. Unknown distributions/SONAMEs receive manual
instructions. ABI failures require the appropriate compatible package; do not replace
glibc to make a native package run. Python 3.11+ remains an installer prerequisite and is checked before package inspection.
Uninstall does not check or remove system dependencies or application user data.

## Four packages and one source companion

The Release presents Windows x64 ZIP, Linux x86_64, native Linux ARM64 and Debian 12
ARM64 archives, plus one complete Source-and-Verification ZIP. The latter includes
all four matching preferred application/upstream Source kits, notices, rebuild
instructions, receipts, checksums and the final manifest snapshot. It also retains
all 26 original release artifacts, unchanged, in `original-v0.1.0/`. GitHub's automatic
tag source links still refer to the original tagged source and do not replace the
complete matching kits for corrected binaries.

`deployment/release_support.py` verifies inventory hashes, one consistent corrected
source identity across four platforms, installer/source receipts and all ZIP members.
The ZIP and its external checksum must stay below GitHub's single-asset size limit.
Its contents inventory excludes only itself. No source availability is removed by
consolidating downloads.

`deployment/update/update.json` is hosted on repository master, not uploaded as a
Release asset. See the [updater contract](../deployment/update/README.md), including
the accepted original-client compatibility consequence and separate source/manifest
commit sequence. No executable downloads or installation occur during checking.

The older `deployment/repackage_linux.py` tool reconstructs packaging-only candidates
from original tagged binaries. Its unchanged executables retain the old updater;
it cannot produce this corrected application generation. All four current packages
must be built with the corrected updater source through the normal Deploy workflow.

Public replacement is a separate owner-authorized operation: preserve originals,
replace the four installers, upload the companion, verify public bytes and matching
source access, then retire superseded standalone support assets including the old
Release JSON. Verify repository-manifest retrieval after its accepted commit/push.
Do not move the original tag or invent a 0.1.1 release.
