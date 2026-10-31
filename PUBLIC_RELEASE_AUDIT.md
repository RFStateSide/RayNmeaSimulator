# Public-release audit

**2026-10-31 — Owner accepted all four M2.60 packages after installing and running
Windows x64, Linux x86_64, native ARM64 and Debian12 ARM64. M2.61 authorizes
private ZIP compatibility corrections, source closeout, the exact v0.1.0 tag
and a GitHub DRAFT with matching assets. Public visibility, release publication,
Pages and announcements remain unauthorized.**

## Authoritative audit policy

This is the **only maintained repository audit document**. Put material outstanding
release findings here; retain detailed development evidence under ignored `build/`.
Do not add milestone audit reports, engineering audit inventories/matrices, addenda
or separate `PUBLIC_RELEASE_*` documents. Normal build/deployment, Help, provenance,
licensing, security and test documentation remains valid. Historical implementation
reports are dated evidence, not competing current audits or blanket acceptance.

## Outstanding publication gates

| Gate | Current disposition and required closure |
| --- | --- |
| Historical artwork, rights and source-history screening | **ENGINEERING REVIEW COMPLETE at `0b2801587ddc737faaa78d102f9d1f7945a8c0c8`.** All five reachable commits, 747 distinct blobs and 714 paths were checked against the 679 excluded paths and historical artwork hashes: no excluded content was found. High-confidence private-key/token patterns were absent; this is a scoped screen, not a guarantee against every possible secret. All 101 physical Help screenshots were visually reviewed; the owner approved Ray usernames in paths on 2026-10-31. Approved generic routes, Default and artwork/provenance remain intact. Owner-directed AI artwork use is attested; no exclusive copyright or trademark clearance is invented. The private Archive is unchanged. |
| Applicable source/license delivery | **IMPLEMENTED; FINAL ARTIFACT QUALIFICATION REQUIRED.** Package-derived Qt module coverage now includes GPLv3 Qt Virtual Keyboard when shipped, alongside Qt Charts. Complete source acquisition (including native/Debian12 ARM64 runtime inputs) and per-platform source sealing bind application files/modes, dependency pins/patches, exact Qt source identities, distro source descriptors/patches, notices, actual installer hashes and update receipts. GPL/AGPL 6(d) same-release source delivery is documented; no unimplemented written offer is used. GCC exception/system-library scope is explicit. Qualification results and any unavailable exact source inputs must be resolved before releasing the affected platform. |
| Final distribution decision | **OPEN FOR PUBLICATION.** The owner accepted M2.59, including the unchanged Ray-path screenshots/provenance, and authorized private commit/fast-forward synchronization in M2.59A. Construct final platform packages/source kits and matching update metadata separately from the resulting clean approved commit; the earlier working-tree qualification artifacts are not final release sets. The source sealer refuses a modified tree by default; its local qualification override records modified state. Separate explicit authorization is required for visibility, Pages, public releases/assets and announcements. |
| Recipient access | **EXTERNAL PUBLICATION DEPENDENCY.** Verify anonymous binary, full matching source, notices, update-metadata and online Help access after the separately authorized publication operations; private/local previews cannot prove public delivery. Retain equivalent source access while conveying the binaries and meet the chosen distribution method's duration requirements. |
| Qualification scope | Retain scoped Windows/Linux/native ARM64/Debian12 evidence and measured ABI requirements. Final ARM64 source/package receipts must be produced from the accepted release revision and actual matching build environments; old accepted runtime packages are dependency/ABI evidence, not regenerated v0.1.0 release artifacts. Do not turn representative fixtures or one device's owner acceptance into universal transport/OS/hardware/endurance certification. |

See [provenance](PROVENANCE.md), [third-party notices](THIRD_PARTY_NOTICES.md),
[security](SECURITY.md), [capabilities/support](docs/capabilities-and-support.md)
and [distribution obligations](docs/deployment.md#distribution-obligations).
These retain technical/license detail without creating additional audit documents.

## Private repository migration

The owner authorized an independent private source history on 2026-10-30.
The historical repository is now **RFStateSide/RayNmeaSimulator-Archive**,
repository ID **863221938**; it remains private with GitHub's read-only Archive
feature disabled. Its original master
`c8f2b5c822050ac875368a02b517e38a14a108b4`, complete history, all 34 original
Issues and discussions remain preserved. Full Git/ref/object backups were
independently restored and integrity checked on approved off-host storage.

The new private **RFStateSide/RayNmeaSimulator**, repository ID **1440258051**,
begins at parentless root `4b49f1f665259cbf49ee104d9af55f14b9f273e8`,
with the exact reviewed 699-file source tree
`922a5d9555e13588dd46c54753593a69cfb6d278`. It inherits no historical commits
or refs. The 12 reviewed actionable Issues were recreated; closed/obsolete
historical tickets were not copied. Both administrators retain access; the
owner chose administrators only for the new repository.

Active development uses fresh `RayNmeaSimulator-Active` checkouts on Windows,
Ubuntu and Rpi5_ssd, new build directories and verified clone-local dependencies.
Original checkouts/builds remain in place, explicitly reference the private
Archive and have disabled push URLs to prevent accidental historical pushes.
The migration does not authorize public visibility, releases, tags, artifact
uploads or Pages. Preserve the scoped qualification and delivery gates above.

## Accepted content and deployment disposition

- Exactly six immutable compiled snapshots in Debug and Deploy: **Default,
  Environment / Weather, Navigation / Autopilot, Single Engine, Synthetic AIS
  Demonstration, Twin Engine**. The [source/resource inventory](resources/BUNDLED-CONTENT.md)
  names all six source files. Unapproved saved authoring files and user Templates
  are not implicitly compiled or copied into a package.
- `resources/defaults/defaultLayout.json` is the byte-preserved owner-approved
  Layout, compiled as `:/defaults/defaultLayout.json`. Current and legacy user
  Defaults keep their precedence; no user data is replaced. Loading Templates or
  Default Layout never authorizes external NMEA Sending.
- The BrickSuite-style [Deploy workflow](deployment/README.md) provides stable
  build-relative stage/installer paths, current-attempt status and retained failure
  evidence. A build-only probe verifies the actual application resource objects
  against source bytes before packaging; its inventory/hashes enter build metadata.
- M2.53/M2.53A qualified private Windows/Ubuntu and native ARM64/Debian 12 workflows.
  M2.54 qualified approved content in Windows Debug/Release and private Windows/
  native ARM64 packages, including installed Linux offscreen/X11 loads of all six
  Templates and Default, normal exit and disposable installer preservation.
  The owner subsequently manually approved the six Templates and Default.
- Preserve the original native ARM64 **glibc 2.38 installation failure**. It is not
  a Debian 12 success. The separately built Debian 12 Qt/application replacement
  passed with measured GLIBC 2.35 / GLIBCXX 3.4.30 / CXXABI 1.3.13 and owner-reported
  Boat installation acceptance. M2.53A requalified that baseline without repeating
  Boat acceptance. M2.54's representative native ARM64 candidate still requires
  GLIBC 2.38 and is not a regenerated Debian 12 candidate. Never upgrade Boat's
  OS/system libraries to fit a native candidate.
- Detailed logs, package hashes, installer/runtime checks and preservation evidence
  stay in ignored `build/m253/`, `build/m253a/`, `build/m254/` and `build/m254c/`.
  Fresh clones are not promised those private artifacts. [Deployment notes](docs/deployment.md)
  preserve clean-machine acceptance and remaining platform limits.

## Consolidated engineering findings and boundaries

The retired inventories were baseline-specific, not current exhaustive support
claims. Their useful outstanding findings and later dispositions are retained here:

| Historical finding | Reconciled disposition |
| --- | --- |
| M2.29A codec versus classifier inventory | A PGN mention/fast-packet classifier is not an implemented codec, application widget, TX/RX path or conformance result. Keep pins and actual encoder/parser/factory/dispatcher evidence distinct. Radio/DSC/proprietary/control additions require separately qualified schemas and control-safety scope; do not invent fields or substitute AIS for radio control. Historical counts describe that source revision, not today's application inventory. |
| M2.32 RX precision, short-payload acceptance and positional identity defects | Precision/payload repairs are recorded in M2.32A; named-field identity matching in M2.32B. Later Layout identity/close/destruction protections are recorded in M2.33B/C. Do not carry the original defects forward as unresolved without new evidence, or assume these repairs prove every message/edge case. |
| M2.35 capability comparison | Raymarine message lists are reference scope, not universal simulator support. Distinguish codecs, instruments, optional generators, proprietary variants and specialized protocol services. Later M2.38/M2.39/M2.43 expansions supersede older missing-feature rows; current support is maintained in the capability guide and source. |
| M2.36A D01-D10 and U01 | M2.36B records fixes for tank Capacity/Level synchronization, engine-hours edits, rapid-engine pressure validation, GLL validity, decoded-double precision, torque range, duplicate RMC callbacks, unit metadata and magnetic/true sentence mappings. Its implementation report and tests retain reproduction/repair evidence; no broad binding-framework rewrite was justified. |
| M2.36A P01, positive 130311 | The original partial state was superseded by M2.38's bounded positive instrument RX/TX implementation; negative Weather source identity remains distinct. Do not re-list the old missing-widget finding as current. |
| U02, exhaustive field/NA/wire coverage | **REMAINS BOUNDED.** Representative fixtures and changed-field tests do not exhaust every legacy enum, sentinel, range, mapping or transport combination. Missing dedicated tests are not automatically confirmed defects. |
| L01, GNSS reference/integrity subset | Advanced/repeated 129029 reference/integrity support remains bounded by the implemented codec/UI contract; do not claim arbitrary reference lists or independent integrity preservation. |
| L02, coordinate editing | The established non-AIS Property Editor six-decimal commit policy is distinct from high-precision RX/snapshot/tree paths. Preserve the documented distinction rather than describing all received data as truncated. |
| Snapshot/source lifecycle | Saved RX Layouts are simulation snapshots, not live subscriptions or restored physical CAN-source identity. Generators are optional and manually created, with bounded instance/filter/binding-epoch behavior. Competing/transient source cases and numerical physics are not universally certified by representative tests. |

Actisense Release TX remains disabled and Debug TX experimental while outbound
corruption is unresolved. Signal K publishing is not raw CAN or implemented Viewer
subscription. Explicit instrument **Enable Sending**, individual Enable/Dispatch,
readiness, Stop/F12 and mode/disconnect safeguards remain. Separately authorized
discovery/route services do not authorize instrument Sending or motion. Receive
operation does not promise electrical silence (ACK/protocol management can occur).
Physical scope and later accepted evidence remain in the capability/support guide.

## Retired audit evidence

M2.54C retires five audit reports and eight accompanying CSV inventories/matrices
from the current tree. All originals were hash-preserved under ignored
`build/m254c/retired-audits/`; all remain in unchanged Git history at baseline
`acf8e9892969bcfc41cee7e692df200e94a833cf`:

- `docs/M2.29A-NMEA2000-PGN-Audit.md` and `docs/M2.29A-NMEA2000-PGN-Inventory.csv`.
- `docs/M2.32-RX-Viewer-Functionality-Audit.txt` and `docs/M2.32B-RX-Identity-Generator-Audit.txt`.
- `docs/M2.35-Raymarine-Capability-Gap-Audit.txt` and `docs/M2.36A-Existing-Functionality-Audit.txt`.
- The seven former `docs/audit/` CSVs: M2.35 NMEA0183/NMEA2000 comparisons,
  M2.36A instrument/source-binding/Viewer matrices and repair backlog, and M2.36B
  repair results.

For exact historical detail use `git show <baseline>:<original-path>` in a
preserved historical checkout explicitly pointed at the private Archive. Those
commits are intentionally absent from the independent active repository. The retained
provenance inventory marks those paths removed from the current tree while keeping
their original evidence/hashes. Licensing/provenance documents, notices, technical
guides and historical implementation reports were not indiscriminately retired.
No historical audit or old pending-acceptance label overrides later scoped evidence
or the owner's current acceptance. Further release polish and the open publication
gates above remain for owner review.

## User documentation and local online Help

The user-facing README and About presentation are prepared for v0.1.0. The
52-topic offline manual is also the source for the static online User Guide;
article prose, anchors and registered images remain shared. Local output is
ignored and the validation workflow has no Pages permissions or deployment.
The intended URL is https://rfstateside.github.io/RayNmeaSimulator/; it has not
been published. The owner accepted the M2.59 preparation and scoped
rights/privacy/source-delivery review. Final clean-revision release qualification
and separate publication authorization remain required under the gates above.

## Manual release checking

Help → Check for Updates is an explicit, unauthenticated information check against
the Active repository's official stable GitHub Releases. A private/no-release
response is expected. It never installs, restarts or authorizes Sending. Future
publication must attach measured per-platform packages and the explicitly assembled
`RayNmeaSimulator-update.json` to the same stable Release. Metadata/ABI filtering
does not replace platform qualification, corresponding-source delivery or owner
publication approval. No release, tag, upload or Pages publication is authorized
by private updater qualification.

## Release source preparation

Release wording uses the official repository/release and Online Help URLs without
inventing a publication event. Conditional runtime no-release/unavailable responses
remain valid results of the actual endpoint. The shared 52-topic Help source, six
bundled Templates and approved Default remain authoritative.

Private source-kit qualification uses exact dependency inputs and records a modified
source fingerprint; it cannot stand in for a final approved clean commit. The Linux
Qt Declarative supplier identifies commit `3a714efe0672ae1ba06f09864f64957560c69b91`,
whereas the standard 6.10.3 archive identifies `490e85145b57254624cbcbfd9ca6d1c846a2a474`.
The exact supplier commit and pinned public test-suite submodule were acquired.
Deployed notices matched that source; source sealing verifies every deployed Qt
notice against matching source, allowing only LF/CRLF equivalence. Never substitute
an archive merely because its release version is the same. Windows installer
engine/version/hashes/license and matching Inno Setup/Pascal Script source are
also included in the inventory and source-delivery gate.

Detailed source/artifact inventories, checksums, upstream acquisition identities,
source-restore checks, history/privacy screening and preservation evidence remain
under ignored `build/m259/`. M2.59A authorizes private source commit/push and fast-forward synchronization.
No visibility, publication, tag, release or artifact upload is authorized. A later hosted service, hardware/User Product,
proprietary dependency or materially different toolchain requires a scoped license/
installation-information review; this assessment covers the documented desktop packages.

## Private v0.1.0 draft preparation

The official Windows delivery container is `RayNmeaSimulator-0.1.0-Windows-x64.zip`.
It holds one root-level installer EXE without changing its bytes. Deploy, package
receipts, runtime filename matching and source sealing identify the outer ZIP;
a measured container receipt separately binds the embedded EXE. Existing official
URL, same-release asset/digest/size, platform and Linux ABI checks remain required.
The original accepted M2.60 EXE and all prior evidence remain preserved. Because
runtime ZIP selection required a source correction, final draft assets must be
rebuilt/sealed from the resulting clean source commit; no M2.60 binary is silently
relabeled. The tag must target that exact commit. The owner authorizes a private
draft and its assets, not a published Release or an immutable/public release state.

Owner-specified repository description, website and 15 relevant Topics may be
updated while preserving the private repository ID. The existing official project
page is a website link, not authorization to publish an announcement or edit it.
Current artifact hashes, clean source/restore checks, test results and actual
GitHub draft/asset verification belong under ignored `build/m261/`. Verify all
public binary/source/metadata/Help recipient access only after separate publication
authorization. No unconditional legal, interoperability or hardware certification
is introduced by this private preparation.
