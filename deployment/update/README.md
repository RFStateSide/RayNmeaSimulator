# Release information for Check for Updates

The application checks GitHub's unauthenticated latest **stable** release endpoint
for `RFStateSide/RayNmeaSimulator`, then retrieves only the release asset named
`RayNmeaSimulator-update.json`. No credentials, cookies, installer retrieval,
background schedule, installation or restart is involved. Opening the modeless
Help → Check for Updates... window starts one check. Retry is explicit.
HTTP 404 reports unavailable stable release information, not “up to date”.
HTTP errors/rate limits and the 15-second overall deadline have distinct messages.
Responses are bounded (512 KiB release, 128 KiB metadata), parsed as untrusted JSON,
and release notes are plain text. Only official HTTPS repository release pages
can be opened. Metadata redirects are limited to three official GitHub/CDN hops.

Windows packages use `RayNmeaSimulator-<version>-Windows-x64.zip`. Extract the ZIP and run its enclosed installer. Metadata sizes and SHA-256 identify the outer ZIP; the container receipt identifies the embedded installer. Checking never downloads or installs the package.

## BrickSuite reference and adaptation

BrickSuite's Updater, Help menu, `deployment/update/update.json`, packaging and
UpdaterTest are the primary references. Retain its explicit check/browser action
and exact platform keys. This implementation discovers published Releases instead
of fetching a mutable master-branch manifest: unreleased source changes must not advertise
new release versions. It adds strict identity/URL checks, SemVer prerelease
ordering, size/time bounds and Linux ABI matching. This is original RF StateSide
implementation; BrickSuite product text/code was not copied or relicensed.

## Platform contract

| Executable / runtime | Exact metadata key |
|---|---|
| Windows x64 | `windows-x86_64` |
| Linux x86_64 | `linux-x86_64` |
| Linux ARM64 | `linux-arm64` |
| Debian 12 ARM64 | `linux-arm64-debian12` |

Architecture is the compiled executable target, not physical-host CPU guessing.
Debian 12 ARM64 never falls back to the native ARM64 entry. Linux additionally
requires measured `GLIBC`, `GLIBCXX` and `CXXABI` ceilings no newer than the current
glibc and already-loaded libstdc++ runtime. Missing/unknown ABI or platform fails
closed. This is an ABI filter, not certification of every desktop/system prerequisite;
review the release notes. Unsupported platforms can read official release information
but receive no matching-package recommendation. Matching version alone never implies
that a compatible installer exists. Only a newer stable version is offered.

## Packaging and release procedure

Normal builds do not generate or publish release metadata. The existing explicit
Deploy/package step additionally emits `update-package.json` alongside the installer,
its checksum and build metadata. The receipt uses the CMake project version and
actual artifact name/bytes/SHA-256, with measured package ABI requirements. Existing
package contents, compatibility ceilings and installer/data-preservation policies
are unchanged. Qt Network and its TLS plugins already belong to the runtime stage.

After independently qualified packages from the same approved version exist:

```sh
python -B deployment/update/metadata.py \
  --receipt build/windows-receipt/update-package.json \
  --receipt build/linux-receipt/update-package.json \
  --output build/release-information/RayNmeaSimulator-update.json
```

Use one receipt per available platform; do not invent URLs/hashes or include packages
that were not qualified. The output must be a fresh physical child of `build/`.
It contains `schema: 1`, `repository`, `version` and `downloads` mapping the exact keys
to `{name, url, bytes, sha256, required_abi}` (`required_abi` is Linux-only).
For publication, use stable tag `v<version>` and attach the
actual packages/checksums plus this metadata file to that **same** GitHub Release.
The checker cross-checks every selected entry against that Release's uploaded asset
identity, size, URL and GitHub SHA-256 digest when present. It does not download/hash
the installer; the displayed hash is published release information, not an independent
artifact verification. Preserve matching corresponding source and third-party notices.
Nothing in this procedure publishes Pages, creates a release, uploads assets or changes
repository visibility. Publication requires separate owner authorization.

## Version policy and tests

About, the application and packager derive the installed version from CMake
`PROJECT_VERSION` / `APP_VERSION`. Compare full three-component SemVer: numeric core,
ordered prerelease identifiers, stable greater than prerelease; build metadata does
not change precedence. Optional `v` is accepted only as the release-tag prefix.
A Debug build compares its declared application version, not its commit age.
Drafts/prereleases are not offered; development/prerelease installed versions compare
correctly against stable releases. Malformed versions are rejected.
Run normal focused CTest `update_tests`, `update_metadata_tests`, `help_tests` and
`online_help_tests`. Network tests substitute a transport while retaining the real
production HTTPS URLs; no credential or live marine interface is required.
