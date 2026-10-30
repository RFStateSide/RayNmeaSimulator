# Software and third-party notices

The application license is AGPL-3.0-or-later ([LICENSE](LICENSE)). This does not
relicense dependencies, separately licensed reference material, data or artwork.
The About designation “Public Engineering Reference — AS-IS” is descriptive.

| Component | Evidence and terms | Distribution consideration |
| --- | --- | --- |
| NMEA2000 | Timo Lappalainen / Kave Oy; MIT headers at pinned revision in dependencies/pins.json | Preserve full permission/copyright notice; compiled upstream code is not RF StateSide-owned |
| NMEA0183 | Timo Lappalainen / Kave Oy; MIT; approved Windows patch identified in pins.json | Preserve original notices and identify the local patch |
| NMEA2000_socketCAN | Al Thomason and fork contributors; MIT | Preserve upstream notice and fork/revision provenance |
| SDL2 2.30.9 | SDL authors; zlib license | Preserve license; historical development package test/fonts may carry additional licenses |
| Qt | Qt Company / Qt Project; component-specific commercial or open-source terms | Qt Charts is GPLv3 or commercial, not LGPL; review every shipped module and transitive dependency |
| CANboat diagnostic data | Kees Verruijt and contributors; Apache-2.0 | Retain resources/nmea2000/NOTICE.txt, LICENSE-canboat.txt and generated source/revision/hash metadata |
| Compatibility control icons | DevSec Studio; MIT | Original icons/controls/LICENSE.txt retained |

## RF StateSide internal reuse

The seven About, credential and logging/viewer adaptations listed in PROVENANCE.md
are owner-attested RF StateSide internal code reuse, not presumed external code.
Relevant BrickSuite history/notices were reviewed without finding conflicting
separately owned implementation. RayNmeaSimulator adaptations are AGPL-3.0-or-later;
origin credit is preserved. BrickSuite retains its existing LGPL-3.0-only license.
LICENSES/BrickSuite-LGPL-3.0.txt and LICENSES/GPL-3.0.txt are retained historical
reference license copies, not a license exception for those application files.

Complete principal MIT/zlib notices and the application license are also available
through About -> Licenses and Acknowledgments. Qt SDK/license material and all
licenses required by a final package must be included by the packaging workstream;
this file is not a substitute for complete notices or corresponding-source duties.

The historical libs/ SDL packages and their original upstream files/licenses,
including unifont material, are retained privately in RayNmeaSimulator-Archive;
they are not part of this active source tree. Current SDL source acquisition is
pinned in dependencies/sdl2-*.json. Preserve the license of the actual acquired
version and any assets included in a distribution; do not apply a blanket zlib
statement to all test assets in an upstream package. Development tools (Python, CMake, Ninja,
Pillow) have their own licenses and are not application-owned runtime code.

The boat artwork is owner-directed ChatGPT output, as attested by the owner;
see images/README.md. Retired Icons8/unknown engine artwork is absent from active
resources but remains a historical rights-review item. No new third-party art
with uncertain rights was introduced.
