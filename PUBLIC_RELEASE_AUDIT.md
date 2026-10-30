# Public-release audit

**2026-10-30 — PRIVATE; public release NO-GO.** The owner accepted the six
bundled Templates, Default Layout (eight groups, 35 widgets), existing Template/
Layout behavior, deployment workflow qualification and catalog validator correction.
That acceptance authorizes private source closeout/synchronization, not publication.

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
| Historical artwork and source history | **OPEN.** Current uncertain Icons8/engine bitmaps were retired and replacement controls/logo provenance recorded. Those removals do not clear artwork remaining in Git history. Obtain verified historical permissions or separately owner-approved publication/history remediation. This closeout does not rewrite history. Review historical technical paths, captures and other source-history privacy before publication. |
| Applicable source/license delivery | **OPEN.** Retain AGPL-3.0-or-later project notices and each dependency's terms. Qt Charts uses GPLv3/commercial terms, not LGPL. Before external distribution establish recipient access to the matching corresponding-source, dependency revisions/local patches, build/installation information and applicable notices under the relevant licenses. A private repository URL or license summary alone does not provide that delivery. Check exact shipped/transitive versions against package manifests/SBOMs. |
| Final distribution decision | **OPEN.** Select a clean reviewed source/candidate, review its content/notices and recipient source-delivery method, then obtain explicit owner publication approval. Private candidates and source push are not a public release. No release tags, GitHub Releases, installer/archive uploads or visibility changes are authorized by this closeout. |
| Qualification scope | Publish only the scoped support/compatibility actually established. Do not turn software fixtures, optional/skipped checks, private X11 tests or owner acceptance on one device into universal transport, OS, desktop, hardware or endurance qualification. |

See [provenance](PROVENANCE.md), [third-party notices](THIRD_PARTY_NOTICES.md),
[security](SECURITY.md), [capabilities/support](docs/capabilities-and-support.md)
and [distribution obligations](docs/deployment.md#distribution-obligations).
These retain technical/license detail without creating additional audit documents.

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

For exact historical detail use `git show <baseline>:<original-path>`. The retained
provenance inventory marks those paths removed from the current tree while keeping
their original evidence/hashes. Licensing/provenance documents, notices, technical
guides and historical implementation reports were not indiscriminately retired.
No historical audit or old pending-acceptance label overrides later scoped evidence
or the owner's current acceptance. Further release polish and the open publication
gates above remain for owner review.
