# Development validation

Use [CMake](build-cmake.md) and [the focused test guide](../tests/README.md).
CMake/CTest is the supported build system; old project recipes have been retired.
Existing historical reports and superseded instructions remain under history.

Application Debug/Release and a dedicated Tests Debug configuration use separate
directories. Production objects never receive RAYNMEA_TESTING. Test/helper objects
are compiled separately, using isolated INI settings and test credential namespaces.
--no-nmea alone does not isolate an ordinary production executable's preferences.

For routine changes select only relevant registered tests or labels. The portable
cmake-dev.py runner lists CTest JSON inventory, builds REQUIRED_FILES prerequisites
and runs the exact requested selection. It refuses implicit Run All and refuses to
repurpose an existing unmanaged/Qt Creator build directory. CTest discovery alone
does not build targets or qualify them. Use the existing launcher when debugging
a particular QtTest function/data row; retain profile/runtime isolation.

Real-entry startup smoke checks identity, resources and stopped Sending. Parity
compares embedded bytes and curated Templates and checks transport/build policies.
Template authoring tests exercise disposable CMake projects, CRUD/backup/safety,
resource addition/rename/deletion/empty catalog and Release read-only behavior.
Python guards remain independent of project recipes; optional platform/replay skips
must be reported separately from passing assertions and physical qualification.

Manual acceptance uses cmake-dev.py run with RayNmeaSimulator-isolated, a fresh
profile and --no-nmea. --smoke selects the bounded real-entry startup check. The
Windows route acceptance fixture uses memory ports/credentials and volatile HKCU,
not a chartplotter. No helper starts network_can_peer or an external CAN sender
automatically. Avoid providers/live network services for build validation.

Preserve owner Layouts/Templates/routes, preferences, credentials, installed
applications, SDKs, accepted IDE configurations and CAN/bridge services. Review
new authored Templates before committing or distributing resources. Capture
before/after preservation evidence for qualification that can touch shared stores.

Accepted native Qt Creator build/run/GDB/CTest workflows cover Windows11 x64,
Ubuntu x86_64 and Rpi5_ssd ARM64 (see README/platform guides). Focused historical
CMake cohorts are not full-suite or physical hardware qualification. Rpi5_new is
excluded from build qualification; macOS remains Issue #6. Future packages/runtime
deployment are #18. Do not broaden a small change into an automatic full matrix.

Qt Creator: select native kit/DebugTests, build chosen targets, open Tests > CTest,
clear filter/Deselect All/reselect intended registrations on every reopen. CTest
refresh or project reopen may be needed after switching from testing OFF. Use
CTest only to avoid QtTest duplicates. Inspect Test Results, raw output and
test-evidence; LastTest.log alone is not sufficient for Not Run classification.

Linux credential-dependent tests use the disposable real Secret Service fixture
described in tests/README.md. Never point regression tests at the owner's session
bus or keyring. Missing fixture prerequisites are failures, not permission to use
desktop credentials. Windows credential namespacing is unchanged.

Ubuntu's historical acceptance-checkout directory and existing DEPS_ROOT remain
an unresolved machine-local cleanup item for installer/deployment preparation.
Portable dependency provisioning does not resolve that concern. Do not rename
the checkout, migrate roots or replace accepted IDE settings as part of tests.
