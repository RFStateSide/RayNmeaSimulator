# Software and third-party notices

The application license is AGPL-3.0-or-later ([LICENSE](LICENSE)). This does not
relicense dependencies, separately licensed reference material, data or artwork.
The About designation “Public Engineering Reference — AS-IS” is descriptive.

| Component | Evidence and terms | Distribution consideration |
| --- | --- | --- |
| NMEA2000 | Timo Lappalainen / Kave Oy; MIT headers at pinned revision in dependencies/pins.json | Preserve full permission/copyright notice; compiled upstream code is not RF StateSide-owned |
| NMEA0183 | Timo Lappalainen / Kave Oy; MIT; approved Windows patch identified in pins.json | Preserve original notices and identify the local patch |
| NMEA2000_socketCAN | Al Thomason and fork contributors; MIT | Preserve upstream notice and fork/revision provenance |
| SDL2 (version recorded per package) | SDL authors; zlib license | Preserve license; historical development package test/fonts may carry additional licenses |
| Qt | Qt Company / Qt Project; component-specific commercial or open-source terms | Qt Charts and Qt Virtual Keyboard are GPLv3 or commercial, not LGPL; shipped modules and transitive dependencies are inventoried per package |
| CANboat diagnostic data | Kees Verruijt and contributors; Apache-2.0 | Retain resources/nmea2000/NOTICE.txt, LICENSE-canboat.txt and generated source/revision/hash metadata |
| Inno Setup installer engine | Jordan Russell, Martijn Laan and upstream contributors; permissive Inno Setup license, retained embedded notices | Actual engine version/hashes and full license accompany Windows packages; matching upstream source and Pascal Script submodule are included in the source kit |
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
through About -> Licenses and Acknowledgments. Package `licenses/`, `redistribution.json` and the matching source archive provide
component notices, supplier records and source materials. See [source delivery](docs/source-delivery.md)
for the distribution method; a summary is not a substitute for those materials.

The historical libs/ SDL packages and their original upstream files/licenses,
including unifont material, are retained privately in RayNmeaSimulator-Archive;
they are not part of this active source tree. Current SDL source acquisition is
pinned in dependencies/sdl2-*.json. Preserve the license of the actual acquired
version and any assets included in a distribution; do not apply a blanket zlib
statement to all test assets in an upstream package. Development tools (Python, CMake, Ninja,
Pillow) have their own licenses and are not application-owned runtime code.

The boat artwork is owner-directed ChatGPT output, as attested by the owner;
see images/README.md. Retired Icons8/unknown engine artwork is absent from active
resources and every reachable Active commit; it remains in the private Archive history. No new third-party art
with uncertain rights was introduced.

## Runtime selection and complete sources

Windows x64 uses Qt 6.10.3, SDL2 2.30.9, GCC 13.1.0 runtime DLLs and
mingw-w64/winpthreads 11.0.1. GCC libgcc/libstdc++ use GPL with GCC Runtime
Library Exception 3.1; the independent runtime DLLs retain those notices.
Winpthreads retains its MIT/BSD notices. Linux x86_64 Qt 6.10.3 also deploys
Qt Declarative, Wayland and Virtual Keyboard; Virtual Keyboard's GPL notice
is required even though it is loaded as an input plugin rather than requested
by application CMake. Its bundled ICU 73.2 uses ICU/Unicode terms.

Native ARM64 and Debian12 ARM64 use separately built Qt 6.10.2 runtimes. Their
SDL2, ICU, Brotli, zlib, Zstandard and PCRE2 versions/patches depend on that
baseline; use each package's `redistribution.json` and complete distro copyright
files, not the Windows versions above. Aggregate distro copyright expressions
can also describe tests/tools that are not linked into a runtime library.
The matching source kit retains the exact distribution source descriptors,
original archives, patches and per-file notices rather than assigning one
blanket license to an entire upstream source package.
