# Repository-hosted update information

RayNmeaSimulator retrieves the bounded JSON manifest at:

`https://raw.githubusercontent.com/RFStateSide/RayNmeaSimulator/master/deployment/update/update.json`

The maintained source path is `deployment/update/update.json`. This follows
BrickSuite's repository-hosted manifest architecture. No update JSON attachment is
required on a GitHub Release. After validating the manifest, the application checks
the latest official stable Release through GitHub's unauthenticated latest endpoint.
A newer package must also match the Release asset name, URL, size and available
GitHub SHA-256 digest. A manifest cannot advertise an unpublished/draft release or silently select a stale historical release.

Remote JSON is untrusted: HTTPS and exact repository identity, SemVer, fixed
platform filenames, positive bounded sizes, SHA-256 syntax, official URLs and Linux
ABI requirements are checked. The total deadline is 15 seconds; manifest and Release
responses are limited to 128 KiB and 512 KiB. Redirects, credentials and cookies are
not accepted. Release notes display as plain text. No installer is downloaded,
installed or launched. Only an explicit browser action opens the verified Release
page. HTTP failures, missing/malformed manifests and incompatible packages have
separate safe outcomes.

| Package | Manifest platform |
|---|---|
| Windows x64 ZIP | `windows-x86_64` |
| Linux x86_64 | `linux-x86_64` |
| Native Linux ARM64 | `linux-arm64` |
| Debian 12 ARM64 | `linux-arm64-debian12` |

The Windows checksum identifies the outer ZIP; its container receipt identifies
the enclosed installer. Linux requires all three measured ABI families: GLIBC,
GLIBCXX and CXXABI. Debian 12 ARM64 never falls back to the native ARM64 entry.
Unknown architectures or unmeasurable requirements do not select a package.

## Generate from verified artifacts

Deploy writes a local `update-package.json` receipt for each actual package. Merge
all four verified receipts, not guessed filenames/checksums:

```sh
python3 deployment/update/metadata.py \
  --receipt build/windows/deploy/installer/update-package.json \
  --receipt build/linux/deploy/installer/update-package.json \
  --receipt build/arm64/deploy/installer/update-package.json \
  --receipt build/debian12/deploy/installer/update-package.json \
  --output build/release-information/update.json
```

Verify each receipt against the actual distribution file before copying the result
to `deployment/update/update.json`. Generation does not commit or publish. The CLI
refuses to overwrite evidence. Keep the build-source commit/tree separate from the
later manifest commit: hashing a package inside the source commit used to build
that same package would create a circular dependency. Source kits retain the exact
build snapshot; the companion retains the final distribution-manifest snapshot.

## Existing v0.1.0 clients

Original v0.1.0 executables use Release-attached `RayNmeaSimulator-update.json`.
Removing that attachment does not change their code. They can still report Current
while the latest release is 0.1.0, but future newer releases without the legacy
attachment cannot provide those clients a matching update package. Manually install
a corrected package to obtain repository-hosted checking. Repackaged 0.1.0 is not a
newer semantic version, so it is not offered as a newer version to 0.1.0 clients.

Run focused CTest `update_tests`, `update_metadata_tests`, `help_tests` and
`online_help_tests`. Transport fixtures exercise the actual checker at the canonical
HTTPS URLs without fetching installers or sending physical NMEA traffic. A positive
live master-manifest check remains a publication verification step after its source
commit is accepted and pushed.
