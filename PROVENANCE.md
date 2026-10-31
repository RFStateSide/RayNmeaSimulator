# Source and asset provenance

This inventory distinguishes project authorship from licensed reuse. A copyright
header, repository location, generated output or rewrite alone is not evidence of
exclusive ownership. The per-file inventory records baseline hashes, introduction
commits, evidence, confidence, license and current disposition.

## Evidence and scope

Historical reports and introduction-commit identifiers refer to the privately
retained RFStateSide/RayNmeaSimulator-Archive engineering history. They do not
assert ancestry in the independent active repository. The active inventory covers
this source tree; historical SDK/report registrations are retained with the archive.
Current build instructions, dependency pins and required notices remain in this tree.

Original application development is evidenced by the existing RF StateSide AGPL
notices, maintainer Git introduction/change history and the milestone engineering
reports. Later engineering additions use their evidenced introduction year; older
2024 notices retain that year. These records are engineering provenance, not an
independent legal chain-of-title certification.

The approved source header is applied to project code, including seven RF StateSide
internal adaptations from the owner's BrickSuite development: src/aboutdialog.cpp,
src/aboutlinks.h, src/credentialstore.cpp, src/applicationlog.cpp/.h and
src/logviewer.cpp/.h. On 2026-10-28 the owner attested ownership and confirmed the
explicit instructions to adapt these implementations for RayNmeaSimulator.

The local BrickSuite reference (9409adfea3f82c482cfe1d318dbacbce1c4383f7) and
the newer GitHub reference (1e2b8f870200b634806ac37dc3629049162a7915), relevant
file histories (17 commits, RayJr2), source notices and prior adaptation reports
were reviewed. No conflicting separately owned contribution or embedded third-party
implementation was identified in those paths. Qt/OS APIs remain separately licensed
dependencies, not imported ownership. These adaptations carry RayNmeaSimulator's
AGPL-3.0-or-later header and retain explicit BrickSuite origin credit. BrickSuite's
existing LGPL license, source and repository are unchanged. Its historical license
copies remain reference evidence, not a separate LGPL license claim on these seven
RayNmeaSimulator files. No implementation rewrite was necessary.

Qt Designer forms are project-authored form data; generated UIC/MOC/RCC build
outputs are not authored application source. Generated CANboat-derived lookup data
retains Apache-2.0 provenance and its source metadata, not the application license.
JSON is intentionally not given invalid comment headers.

## Artwork

The owner stated on 2026-10-28 that ChatGPT created the approved boat logo for them.
The supplied original and its generated PNG/ICO/ICNS derivatives are retained;
images/README.md records the source hash and reproducible derivation. This records
owner-directed AI generation and permission for application use, not a promise of
exclusive copyright or absence of similar outputs elsewhere.

Historical Icons8-named and unverified engine bitmap assets are removed from the
current source/resource set. Setup now uses an independently drawn Qt controls glyph; Exit and Refresh use Qt standard icons, with
Qt's applicable licensing; no uncertain bitmap was traced or used as the basis of
a derivative. Retained compatibility controls have their original MIT notice.
Git history still contains retired artwork: removing current files does not clear
historical publication rights. Separate owner-approved history/publication
remediation or verified historical permissions are required.

## Examples and historical evidence

The owner confirmed the bundled river routes, default boat position and similar
geographic examples are acceptable for release review. They are retained, not
classified as private merely because they contain coordinates. This is not blanket
permission to include future vessel captures, personal metadata or credentials.

Historical engineering evidence is preserved privately in the archive repository,
its original Git history and owner-retained engineering evidence. Redundant audit reports/matrices
were consolidated into the sole [public-release audit](PUBLIC_RELEASE_AUDIT.md);
their original hashes/history remain recorded, not rewritten. These records contain machine-specific
technical paths and network setup examples; they require a deliberate publication
review, independently of executable packaging. The inventory does not publish
recovered credential values or private owner data.

## Distribution boundaries

Historical libs/ SDL development packages and their upstream test/font notices
are retained in the private archive, outside this active source tree. Current builds
use explicit pinned provisioning. SDL and all other dependencies retain their own
licenses; see dependencies/ and THIRD_PARTY_NOTICES.md.
CMake installs the application and selected desktop/icon files; dependency DLLs,
frameworks, complete source/notices and final installer contents require packaging
qualification. Use a reviewed source checkout for packaging. The explicit
six-Template allowlist and approved compiled Default are identical in development
and Deploy; the build-only resource probe checks the compiled bytes. Other saved
authoring files are not implicitly compiled. Never copy private authored Templates,
captures or test profiles into a package. See resources/BUNDLED-CONTENT.md.

Run `python scripts/check-provenance.py` for read-only registration, header and
JSON/XML syntax checks. Inventory raw hashes describe the audited working copy;
Git blob IDs and introduction commits provide canonical history references.
The inventory deliberately omits its own self-referential content hash.

## Help framework adaptation — M2.51A

The offline Help catalog/dialog adapts RF StateSide BrickSuite's HelpManager and
HelpDialog at `1e2b8f870200b634806ac37dc3629049162a7915`, verified as current master
during the read-only reference review. The modeless tree/browser layout, local
HTML, search, history, theme formatting, geometry and responsive-image patterns
retain origin credit in source. Owner authorization covers this internal reuse;
RayNmeaSimulator adaptations use AGPL-3.0-or-later. Resource allowlisting and the
image zoom window are application-specific additions. BrickSuite is unchanged.
Phase 1 articles are RF StateSide placeholder content, not a completed manual.
## Illustrated Help manual (M2.51B)

The 49 Help articles are RF StateSide instructions authored against the current
RayNmeaSimulator source/UI. BrickSuite's Help is the owner-authorized internal
instructional/visual reference; BrickSuite product procedures were not copied.
Native instructional captures use Darkeum, disposable settings and synthetic
demonstration values. `resources/help/images/capture-provenance.json` records
capture method, context, dimensions and SHA-256 for each screenshot. The explicit
`help_manual_capture` helper is test infrastructure, not production behavior.
Screenshots depict the application's existing Qt/third-party UI and retained
assets; their existing notices remain applicable. They do not establish physical
hardware qualification or resolve historical artwork/public-history publication
and final distribution-notice gates.

## Shared online manual and presentation

The README/About presentation and static online User Guide use the owner's
current BrickSuite README, About and shared-help Pages architecture as primary
references at `1e2b8f870200b634806ac37dc3629049162a7915`. The new static generator,
responsive CSS and browser search/image viewer are original RayNmeaSimulator
implementation under AGPL-3.0-or-later; BrickSuite source/license is unchanged.
The accepted 51 HTML articles, topic inventory and Qt image resource aliases are
shared with offline Help. Generated pages are not separately maintained source.
The revised About illustration is an actual native Darkeum QWidget capture from
the isolated About regression, recorded in the screenshot provenance manifest.

## Check for Updates

The on-demand update dialog, bounded release client, controlled network fixtures
and packaging receipt/metadata tools are original RF StateSide implementation.
The primary engineering reference was BrickSuite's `src/services/Updater`, Help
menu, `UpdaterTest` and `deployment/update` contract in the local reference checkout
at revision `1e2b8f870200b634806ac37dc3629049162a7915`. Its check/notify/browser
conventions and exact-platform policy informed this implementation; no BrickSuite
product content or separately licensed implementation was copied or relicensed.
The new Check for Updates instructions extend the shared offline/online manual to
52 topics while retaining the accepted 51 original articles and screenshots.
Original project additions retain AGPL-3.0-or-later notices.
