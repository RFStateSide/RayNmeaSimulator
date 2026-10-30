# v0.1.0 bundled content

The owner-approved compiled Template catalog is explicit in
`templates/templates.cmake`, identical for Debug and private Deploy builds:

| Display name | Source | Resource |
| --- | --- | --- |
| Default | `templates/Default.json` | `:/templates/Default.json` |
| Environment / Weather | `templates/environment-weather.json` | `:/templates/environment-weather.json` |
| Navigation / Autopilot | `templates/navigation-autopilot.json` | `:/templates/navigation-autopilot.json` |
| Single Engine | `templates/single-engine.json` | `:/templates/single-engine.json` |
| Synthetic AIS Demonstration | `templates/synthetic-ais.json` | `:/templates/synthetic-ais.json` |
| Twin Engine | `templates/twin-engine.json` | `:/templates/twin-engine.json` |

Names, descriptions, IDs and serialized configurations are retained from those
approved files. Other JSON files in the source authoring directory are excluded
from compiled snapshots; they are not deleted. Debug source authoring remains
available independently. Adding/renaming a source does not add another approved
Template to the compiled catalog. An intentional catalog change requires review.

The Default Layout source is `defaults/defaultLayout.json`, copied byte-for-byte
from the owner's approved saved Layout. CMake compiles it as
`:/defaults/defaultLayout.json`. It is a concrete format-2 Layout, separate from
the Templates. **Layout → Load Default Layout** retains the existing precedence:
an existing user Default first, then the compiled Default. Loading does not
authorize external Sending; saved Enable/Dispatch states remain document settings.
Bundled resources are read-only. User Templates/Layouts remain separate and editable.

The historical `tests/fixtures/defaultLayout.json` remains a synthetic fixture
for unrelated unit scenarios and is excluded from application/package resources.
`bundled_content_tests`, `cmake_parity_tests` and `bundled_resource_catalog` link
the actual application resources, including the approved Default.
`raynmea_bundled_content_probe` is a build-only Qt Core verifier, checked by the
packager and never shipped. Its output records names, descriptions, identities,
source/resource paths and hashes in package `build-metadata.json`.
