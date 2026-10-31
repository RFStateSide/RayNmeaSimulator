# Public-release audit

**2026-10-31 — PUBLIC RELEASE PASS: RayNmeaSimulator v0.1.0.** The owner authorized
public publication in M2.62. The active repository is PUBLIC, the approved stable
Release is published, and the common-source Online Help is live. Anonymous source,
complete binary/source downloads and live update discovery have been verified.
The historical Archive remains **PRIVATE**. Unqualified future distributions remain
**NO-GO** until their applicable rights, matching-source and runtime gates are met.

## Authoritative audit policy

This is the **only maintained repository audit document**. Put material outstanding
release findings here; retain detailed development evidence under ignored `build/`.
Do not add milestone audit reports, engineering audit inventories/matrices, addenda
or separate `PUBLIC_RELEASE_*` documents. Normal build/deployment, Help, provenance,
licensing, security and test documentation remains valid. Historical implementation
reports are dated evidence, not competing current audits or blanket acceptance.

## Publication verification and continuing obligations

| Gate | Verified disposition on 2026-10-31 |
| --- | --- |
| Historical artwork, rights and source-history screening | **PASS for the approved release.** The eight reachable release commits, 800 unique blobs and 807 object paths were screened against 679 excluded paths and historical artwork hashes: no excluded content or high-confidence private-key/token pattern was found. The earlier visual review of all 101 physical Help screenshots and owner approval of Ray-path screenshots remain applicable. The 102 registered image resources include aliases. Post-tag Pages/documentation additions were reviewed separately. This scoped screen is not a guarantee against every possible secret or exclusive artwork/trademark rights. |
| Applicable source/license delivery | **PASS for these four approved desktop packages.** Matching complete platform Source kits, notices, receipts and checksums provide corresponding-source delivery beside their binaries. Exact upstream inputs and source-restore checks from M2.61 were retained; all 26 public assets match the accepted bytes. Qt Charts and Virtual Keyboard coverage, distro source/patches, compiler/runtime and installer-source obligations remain documented. Continue equivalent GPL/AGPL 6(d) source access while conveying binaries and meet the chosen delivery method's duration requirements. No unimplemented written offer or blanket legal certification is asserted. |
| Distribution decision | **AUTHORIZED AND COMPLETED.** The existing approved Release ID 430092960 was published unchanged, not recreated. Tag v0.1.0 remains on the clean approved application revision below. No binary, Source kit, receipt or update metadata was rebuilt or substituted for publication. |
| Recipient access | **PASS.** An unauthenticated fresh clone recovered all 718 approved release files. Anonymous clients retrieved all 26 assets and verified every SHA-256/size; aggregate SHA256SUMS covers the other 25 assets. README, license, tag, release page and public HTTPS Help resolve without authentication. |
| Qualification scope | Owner acceptance of all four installed M2.60 platforms and clean M2.61 Windows/Linux/native ARM64/Debian12 source/package qualification are retained. Publication checks add recipient delivery and real update discovery; they do not claim universal device, transport, OS, endurance or safety certification. |

See [provenance](PROVENANCE.md), [third-party notices](THIRD_PARTY_NOTICES.md),
[security](SECURITY.md), [capabilities/support](docs/capabilities-and-support.md)
and [distribution obligations](docs/deployment.md#distribution-obligations).

## Private repository migration

The owner authorized an independent private source history on 2026-10-30.
The historical repository is now **RFStateSide/RayNmeaSimulator-Archive**,
repository ID **863221938**; it remains private with GitHub's read-only Archive
feature disabled. Its original master
`c8f2b5c822050ac875368a02b517e38a14a108b4`, complete history, all 34 original
Issues and discussions remain preserved. Full Git/ref/object backups were
independently restored and integrity checked on approved off-host storage.

The new independent **RFStateSide/RayNmeaSimulator**, repository ID **1440258051**,
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
The migration itself did not authorize publication. M2.62 separately authorized
the active repository, existing v0.1.0 Release and Online Help publication now
verified above; it did not authorize historical Archive publication or a website announcement.

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
or the owner's current acceptance. Current publication status and continuing
source-delivery obligations above govern this release.

## Published Online Help

The shared 52-topic offline manual is published at
<https://rfstateside.github.io/RayNmeaSimulator/>. All 52 topic pages and 102
registered images, local styles/scripts and generated manifest passed anonymous
HTTPS resource/hash checks against the accepted generator. Desktop/mobile browser
checks covered Contents, topic/home navigation, full-text search with Arrow/Enter
and Escape, image enlargement/100%/Escape, and responsive layout without horizontal
overflow. The build-only `.nojekyll` marker is not served as a public resource by
GitHub Pages; it is not a broken user link. Offline Help and its resources are unchanged.

The separate Publish User Guide workflow builds the same manual and uses pinned
official actions. Only its deploy job receives Pages/OIDC write permissions;
the job and github-pages environment permit master deployment only. The existing
validation workflow remains read-only. The first Pages deployment completed
successfully; generated output remains ignored locally and is not source content.

## Manual release checking

Help → Check for Updates performs an explicit, unauthenticated check of the official
stable GitHub Release. The unchanged approved Windows Release GUI displayed
installed/latest 0.1.0 and “You have the latest published stable version.” in an
isolated --no-nmea profile. The actual checker, compiled as a private controlled
0.0.9 test client, retrieved the live Release and update metadata, identified
v0.1.0 and selected the exact Windows ZIP and checksum. Controlled Linux host/ABI
cases against that published metadata accepted appropriate native/Debian12 packages
and rejected an incompatible native ARM64 fallback for Debian12.

Recorded requests retrieved release information/metadata only, never an installer
or binary archive. Existing isolated memory-transport and movement-preservation
regressions passed. No owner Sending, movement, settings or marine session was
used, reconfigured or interrupted. Runtime no-release/unavailable results remain
conditional endpoint outcomes, not permanent product descriptions.

## Release source preparation

Release wording uses the official repository/release and Online Help URLs without
inventing a publication event. Conditional runtime no-release/unavailable responses
remain valid results of the actual endpoint. The shared 52-topic Help source, six
bundled Templates and approved Default remain authoritative.

Initial private source-kit qualification recorded a modified source fingerprint;
it was superseded by M2.61 clean-revision sealing and independent restore checks
for all four approved Source kits. The Linux
Qt Declarative supplier identifies commit `3a714efe0672ae1ba06f09864f64957560c69b91`,
whereas the standard 6.10.3 archive identifies `490e85145b57254624cbcbfd9ca6d1c846a2a474`.
The exact supplier commit and pinned public test-suite submodule were acquired.
Deployed notices matched that source; source sealing verifies every deployed Qt
notice against matching source, allowing only LF/CRLF equivalence. Never substitute
an archive merely because its release version is the same. Windows installer
engine/version/hashes/license and matching Inno Setup/Pascal Script source are
also included in the inventory and source-delivery gate.

Detailed source/artifact inventories, upstream identities, independent source-restore
checks and preservation evidence remain under ignored build directories. M2.59A
source preparation, M2.60/M2.61 release-set qualification and M2.62 publication are
distinct accepted stages. A later hosted service, hardware/User Product,
proprietary dependency or materially different toolchain requires a scoped license/
installation-information review; this assessment covers the documented desktop packages.

## Published v0.1.0 release set

- Public repository: <https://github.com/RFStateSide/RayNmeaSimulator>, ID **1440258051**.
- Stable Release: <https://github.com/RFStateSide/RayNmeaSimulator/releases/tag/v0.1.0>,
  ID **430092960**, published **2026-10-31 18:45:06 UTC**; neither draft nor prerelease.
- Approved application/source revision: `87cd0912b3e10777e610a82a78156484f66a2c12`.
- Annotated tag object: `daab0924a934c976a1145da2a9e557465e4a1356`; peels to that revision.
- Canonical release-file manifest: `d02266900f852e2117547b3337951bdbe8a3007768fe28dab1c450bcd9168b71`.
- Application source fingerprint: `efe39d514940ef43aa19d9939b42785846db697384e5bbcb54043ba97664e733`.

All 26 accepted asset names, sizes and hashes were preserved. Downloads use
`https://github.com/RFStateSide/RayNmeaSimulator/releases/download/v0.1.0/`.
The official Windows ZIP contains the unchanged approved installer; its contents
receipt binds the inner EXE. The original M2.60 EXE and previous evidence remain
preserved privately. Each platform Source kit restores the 718 approved application
files/modes and complete matching upstream inputs; GitHub's automatic source ZIPs
are not substitutes for these complete kits. Post-tag Pages configuration and this
publication record do not move the accepted tag or change published package bytes.

| Published asset | SHA-256 |
| --- | --- |
| `RayNmeaSimulator-0.1.0-Linux-aarch64-Debian12.tar.gz` | `8b77face77692334a59ceacdc1fd384e1abf2a9df8eef79fe4ccf983bd08ba76` |
| `RayNmeaSimulator-0.1.0-Linux-aarch64-Debian12.tar.gz.sha256` | `1d61fba595bb9cf4f490e16e713ae814861f01e6318d0ac7e8340cc871a05f91` |
| `RayNmeaSimulator-0.1.0-Linux-aarch64.tar.gz` | `9bcf0c7ba35dc80519374ad1da79c2d13883a79cf0d90d86c108ba74dc1895f5` |
| `RayNmeaSimulator-0.1.0-Linux-aarch64.tar.gz.sha256` | `c7a22bd750ce4ef76ecbaa2a3e253084d9775ae7569464d62ba791eed6c673e8` |
| `RayNmeaSimulator-0.1.0-linux-arm64-debian12-release-set.json` | `ac71321ae624249a81e5aadad8385f7b859e767a7982a8f19a79fcc060ab5db5` |
| `RayNmeaSimulator-0.1.0-linux-arm64-debian12-Source.tar.gz` | `bca7232eb4968434aa6ff9fe795ab85836d54d24fc3948a7ea355783d1e899cb` |
| `RayNmeaSimulator-0.1.0-linux-arm64-debian12-Source.tar.gz.sha256` | `462297dcb4809817c7c8753c2c589baaf693e9306846d33974126fab3e0f7b34` |
| `RayNmeaSimulator-0.1.0-linux-arm64-release-set.json` | `a7669afcd1d3b3668b24136b678b5f8bec061876977eb358dd8fb3837de52569` |
| `RayNmeaSimulator-0.1.0-linux-arm64-Source.tar.gz` | `d0b9d174343a7b7462aee90ecef179d267e9318d70e5434634b1a586e778d682` |
| `RayNmeaSimulator-0.1.0-linux-arm64-Source.tar.gz.sha256` | `6c132d8bc6902ea6b4d2158d400ee690e689086de17285f1e5d91509a22e1af2` |
| `RayNmeaSimulator-0.1.0-linux-x86_64-release-set.json` | `abfc57fde62c3c2e49e41efc1ffe5abb368d73d8d5c38c387513911da7abd857` |
| `RayNmeaSimulator-0.1.0-linux-x86_64-Source.tar.gz` | `977a9d7cc135853b34810416aa3164d621a9cdb92a036482e341a20420a952a1` |
| `RayNmeaSimulator-0.1.0-linux-x86_64-Source.tar.gz.sha256` | `f3bc4e0014fb74e58e1276d9636138d9c1ae1b5e43c8fdfdfe48dd9c96ef82dd` |
| `RayNmeaSimulator-0.1.0-Linux-x86_64.tar.gz` | `d509864d9867da871bfd789a6b05d97e1ddf06677ab123f5a35cbdb5230bd26c` |
| `RayNmeaSimulator-0.1.0-Linux-x86_64.tar.gz.sha256` | `42d83ad9b56d2a6c4abc5c7bf87f90de1410d45312d9b6bce15c93ecc9159df5` |
| `RayNmeaSimulator-0.1.0-Notices.tar.gz` | `1691399a43e3739beff7d714ae66d2720e64371eca4d80c7d98bbb69443615ed` |
| `RayNmeaSimulator-0.1.0-Notices.tar.gz.sha256` | `58b00618a08e6c74e0afdf9179fbc59e64c933f7731fa4ed63a15880ca465706` |
| `RayNmeaSimulator-0.1.0-Windows-x64.zip` | `f61e071f1108859b7c3f28c6e21797bb454efaa5084f40499987a7d0d07b387f` |
| `RayNmeaSimulator-0.1.0-Windows-x64.zip.contents.json` | `084f819e17987bb9138f6d1f6bae3902b48acf3ac81517935b4a0f1fff819e41` |
| `RayNmeaSimulator-0.1.0-Windows-x64.zip.sha256` | `077fa55191ed799947a12525d0c739d836b6a398cc1e9fd057467519e81d15ae` |
| `RayNmeaSimulator-0.1.0-windows-x86_64-release-set.json` | `c9dd58d259429cbf706607052face95c25561163eb3bd66b483fccc5c382fc0f` |
| `RayNmeaSimulator-0.1.0-windows-x86_64-Source.tar.gz` | `6f726da39134f245d535aecf5fe834392af4a03962346baa4bc947051af7c0cc` |
| `RayNmeaSimulator-0.1.0-windows-x86_64-Source.tar.gz.sha256` | `c5e4da3de562f497ebc1b39602cd2b991fb5dac13f5d1addc83b1cdefd301ad3` |
| `RayNmeaSimulator-update.json` | `6ffcf9b0e0041308d366052ec3d1791245db5c613750a091998e608afbd10dab` |
| `RayNmeaSimulator-update.json.sha256` | `5956a729a065a0b3586f7bbfbf393a25ec345c54eea28c6058b29a7c8d452900` |
| `SHA256SUMS.txt` | `4b5d8724395b0c92e1e4a217deafd88d0d076bf2038033eefda2ac0a16556a19` |

## Access, protection and website follow-up

RFStateSide organization ownership is unchanged. The two expected organization
administrators retain admin access; no additional direct write collaborators,
repository teams, deploy keys or installed organization GitHub Apps were found.
Actions default token permissions remain read-only with pull-request approval
permission disabled. Pages publishing rights are limited to the dedicated deploy
job and master environment policy; no organization-wide security policy changed.

Public clone, source/read routes and pull-request entry routes resolve, and public
forking is enabled. No dummy fork or pull request was created; normal GitHub
account/authentication requirements still apply to contributions. **master has no
branch protection and no repository rulesets.** This limitation was reported, not
silently changed. Consider owner-approved protection against force pushes/deletion
and required checks for future development.
The repository API also reports secret scanning, secret-scanning push protection
and Dependabot security updates disabled. These settings were not changed; consider
owner-reviewed repository protections separately from the completed scoped history
screen, without silently changing organization policy.

The official project page <https://rfstateside.com/projects/raynmeasimulator>
returns HTTP 200. It still contains pre-release download/audit wording and lacks
release/source/Online Help links. A separate owner-approved website edit should
link this Release, repository and Help, replace obsolete pending-publication text,
and align feature claims with the accepted capability guide (including Viewer RX,
manual authorization of Sending and supported protocol/platform limits). No website
content or announcement was changed in this assignment.

These verified publication results are **PUBLIC RELEASE PASS** for the exact
approved v0.1.0 distribution. Continuing source availability, documented operating
limitations and future scoped qualification remain obligations, not promises of
navigation, safety, hardware or legal certification. Detailed evidence remains
under ignored `build/m262/`; this remains the sole maintained tracked audit.
