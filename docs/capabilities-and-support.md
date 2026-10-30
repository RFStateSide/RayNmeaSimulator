# Current capabilities and support

Status reviewed **2026-10-30** for accepted M2.53/M2.53A/M2.54 source closeout.
Earlier platform evidence retains its explicitly dated M2.45 scope below.
This page is the maintained capability/support summary linked from README.
Update its date, source baseline and evidence when accepted behavior changes.
Detailed message definitions remain in source; this is not an exhaustive PGN or
sentence matrix, a certification statement or a promise of future release scope.

## Implemented behavior

- **Instruments and Viewer:** NMEA2000 and NMEA0183 receive handling, matching by
  supported source/instance identity, dynamic instrument creation/update, editable
  canonical properties and property-driven TX. Generator sources are optional;
  their Enable/Dispatch and compatible signal bindings supply dynamic values.
  Viewer does not authorize instrument Sending. Support is message-specific:
  a Diagnostics entry or RX decoder alone does not establish bidirectional support.
  See the [handlers](../src/nmea2000handler.cpp),
  [NMEA0183 path](../src/nmea0183handler.cpp),
  [instrument factory](../src/widgets/expansioninstruments.cpp) and
  [binding controller](../src/maincontroller.cpp).
- **Navigation and routes:** GPX/KML files, bounded passive route reception and the
  qualified active Axiom route-service workflow feed the existing VesselMovement
  Autopilot route selection. Selection alone never starts Autopilot, movement or
  instrument Sending. Existing XTE and navigation bindings work; additional
  APB/RMB mappings remain separate work under [#29](https://github.com/RFStateSide/RayNmeaSimulator-Archive/issues/29).
  The [M2.39 instrument/passive-route report](https://github.com/RFStateSide/RayNmeaSimulator-Archive/blob/c8f2b5c822050ac875368a02b517e38a14a108b4/docs/M2.39-Navigation-Route-Protocol-Expansion.txt)
  predates the [active retrieval qualification](https://github.com/RFStateSide/RayNmeaSimulator-Archive/blob/c8f2b5c822050ac875368a02b517e38a14a108b4/docs/M2.39B7-Active-Axiom-Route-Retrieval.txt)
  and [network integration](https://github.com/RFStateSide/RayNmeaSimulator-Archive/blob/c8f2b5c822050ac875368a02b517e38a14a108b4/docs/M2.39B8-Network-NMEA2000-Bridge.txt).
- **Layouts and Templates:** version 2 persistence, user Template management,
  Setup initialization policies and startup workspace selection are implemented.
  Saved RX Layouts preserve simulation snapshots; RX traffic creates instrument
  widgets while generator sources remain manually added.
  Exactly six owner-approved Templates and the eight-group/35-widget Default Layout
  are bundled; see [their source/resource inventory](../resources/BUNDLED-CONTENT.md).
  Compiled snapshots are read-only in Debug and Release;
  Debug permits source authoring with validation/backups and subsequent resource
  rebuild. A running binary's resources stay immutable. See
  [Layout/Dispatch design](Layout-Templates-and-Generator-Dispatch.txt) and
  [Template authoring evidence](https://github.com/RFStateSide/RayNmeaSimulator-Archive/blob/c8f2b5c822050ac875368a02b517e38a14a108b4/docs/M2.41C-Bundled-Template-Authoring-Editor-UX-report.txt).
- **Diagnostics and logging:** device identity/product metadata, supported explicit
  discovery, qualified proprietary interpretation and opaque unsupported-message
  diagnostics coexist with the existing NMEA Viewer. Application logging and
  optional separate traffic recording are implemented; traffic recording is off
  by default. See [Diagnostics evidence](https://github.com/RFStateSide/RayNmeaSimulator-Archive/blob/c8f2b5c822050ac875368a02b517e38a14a108b4/docs/M2.43C-Device-Identity-PGN-Diagnostics-report.txt),
  [logging](https://github.com/RFStateSide/RayNmeaSimulator-Archive/blob/c8f2b5c822050ac875368a02b517e38a14a108b4/docs/M2.42B-Application-Logging-report.txt) and
  [viewer Clear/Pause correction](https://github.com/RFStateSide/RayNmeaSimulator-Archive/blob/c8f2b5c822050ac875368a02b517e38a14a108b4/docs/M2.42C-Log-Viewer-Clear-Pause-report.txt).

Detailed evidence for expanded instruments includes the dated
[M2.38 additions](https://github.com/RFStateSide/RayNmeaSimulator-Archive/blob/c8f2b5c822050ac875368a02b517e38a14a108b4/docs/M2.38-v0.1.0-NMEA-Capability-Expansion.txt), later M2.39 work above,
and [four qualified PGNs](https://github.com/RFStateSide/RayNmeaSimulator-Archive/blob/c8f2b5c822050ac875368a02b517e38a14a108b4/docs/M2.43D-Four-Bidirectional-Instruments-report.txt) with
[creation/Datum/selection corrections](https://github.com/RFStateSide/RayNmeaSimulator-Archive/blob/c8f2b5c822050ac875368a02b517e38a14a108b4/docs/M2.43D1-Instrument-Creation-Selection-Polish-report.txt).
The proprietary Pilot Heading instrument is the qualified Raymarine 1851/industry 4
variant; it is not generic support for every message sharing that PGN.

## Development platforms and build policies

| Environment qualified in M2.45 | Accepted development workflow |
| --- | --- |
| Windows 11 x64; Qt 6.10.3 MinGW64, GCC 13.1.0, SDL2 2.30.9 | CMake Debug/Release build/run; native GDB; focused CTest in Qt Creator |
| Ubuntu 26.04.1 x86_64; Qt 6.10.3, GCC 15.2, SDL2 2.32.10 | CMake Debug/Release build/run; native GDB; focused CTest in Qt Creator |
| Rpi5_ssd, Debian 13.3 ARM64; Qt 6.10.2 gcc_arm64, GCC 14.2, native SDL2 | Native Debug/Release Qt Creator build/run/GDB; selected Debug CTest and persistent DebugTests workflow |

These are qualified development combinations, not universal OS/ABI support or
clean-machine installer qualification. Rpi5_new remains a Debian 12/OpenPlotter
monitoring computer, excluded from required build qualification; migration is deferred.
Native macOS build and secure storage qualification remain [#6](https://github.com/RFStateSide/RayNmeaSimulator-Archive/issues/6).

Debug and Release retain the same instrument authorization rules. Debug alone
exposes bundled Template source authoring and experimental Actisense TX;
Release enforces bundled read-only resources and Actisense RX-only policy.
[Transport policy source](../src/transportcapabilities.h) is authoritative for
these restrictions. Separate ordinary and test object builds protect normal
settings/credential behavior; test targets use explicit isolated profiles.

## Transport implementation and physical scope

| Interface | Current capability and evidence boundary |
| --- | --- |
| CANable SLCAN | RX/TX physically qualified on Windows and Ubuntu with the tested MKS CANable hardware/firmware; see the [accepted USB-CAN closeout](https://github.com/RFStateSide/RayNmeaSimulator-Archive/issues/26#issuecomment-6376543471). |
| Waveshare USB-CAN-A | Windows physical RX/TX qualified; replacement Ubuntu adapter RX/TX owner accepted. Corrected wiring, 2 Mbaud serial and nonblocking 5 ms frame pacing are retained. The same closeout supersedes earlier pending Ubuntu hardware statements; it does not claim a repeated engineering capture or universal adapter compatibility. |
| Native SocketCAN | Linux implementation with qualified native ARM64 builds and offline driver/session tests. Those wrapped-syscall/fake-backend tests do not qualify a physical HAT/backbone. See [integration and operating boundaries](SOCKETCAN.txt); hardware setup/qualification remains distinct. |
| Network NMEA2000 / RayNetworkBridge | Implemented RX/TX capability, authenticated sessions and bounded route services. M2.39B.8 Windows owner acceptance passed live boat RX/TX, Axiom retrieval, Autopilot and virtual multi-client checks. Scope is the tested client/bridge/device workflow, not every chartplotter or network; see the [bridge guide](../tools/network-can/README.md). |
| Actisense NGT-1 | RX supported. Release TX disabled; Debug TX experimental because outbound corruption is unresolved in [#1](https://github.com/RFStateSide/RayNmeaSimulator-Archive/issues/1). No normal RX/TX qualification claim for its outbound path. |
| Signal K | Publishing implemented with supported mappings; subscription into Viewer is not implemented. It is not raw CAN transport or evidence of physical NMEA2000 delivery. |

Instrument TX requires explicit **Enable Sending**, individual Enable/interval and
transport readiness. Stop/F12, mode/configuration changes and disconnect safeguards
remain. Diagnostics discovery and chartplotter requests use their own explicit,
bounded service authorization; they do not authorize instrument Sending or motion.
Receive operation is not a claim of electrical silence: controllers can ACK and
some transports perform protocol management. No software fixture establishes
physical delivery, firmware compatibility, endurance or queued-frame retraction.
Serial-port selection/refresh is accepted; extending Diagnostics device discovery
to serial CAN adapters is separate [#27](https://github.com/RFStateSide/RayNmeaSimulator-Archive/issues/27).

## Test infrastructure and evidence

CMake-only retirement is committed at `215732cc0af88f8b7cffbf848fa40428c653f71a`;
disposable Linux credential isolation at the reviewed baseline above. Both are
owner accepted. Explicit pinned provisioning was introduced at
`84fadbd4b04d572f5808a486d184e354e8c91c5f`; this is a milestone reference, not a
pending migration. Follow the [CMake guide](build-cmake.md),
[dependency workflow](DEPENDENCIES.txt), [test guide](../tests/README.md) and
[development validation](development-validation.md).

[tests/CMakeLists.txt](../tests/CMakeLists.txt) is the single target/group authority.
The developer runner reads CTest metadata, builds selected prerequisites and
reports actual results. Discovery is not execution. Qt Creator uses CTest-only
selection to avoid duplicates. Profile isolation, owned offline fixtures and
private Linux D-Bus/Secret Service handling keep tests away from normal keyrings.
Missing prerequisites fail explicitly; raw output/QtTest results retain nested
skips. Windows retains test-namespaced Credential Manager coverage.

Evidence is dated and scoped:

- [M2.45C](https://github.com/RFStateSide/RayNmeaSimulator-Archive/blob/c8f2b5c822050ac875368a02b517e38a14a108b4/docs/M2.45C-Parallel-CMake-Implementation-report.txt) records explicit
  source/UI/resource parity, T01–T04 harness repairs and focused Windows/Ubuntu
  cohorts. [M2.45D](https://github.com/RFStateSide/RayNmeaSimulator-Archive/blob/c8f2b5c822050ac875368a02b517e38a14a108b4/docs/M2.45D-Portable-Dependency-Provisioning-report.txt) records
  independent clean clones/new pinned stages and selected configure/build/test checks.
- Later accepted M2.45H qualification: Windows Debug 8 selected CTest passes and
  Release 4; Ubuntu Release 4 and seven complete Debug suites. Its five blocked
  Ubuntu Sending credential cases were subsequently corrected by M2.45I.
- Accepted M2.45I: all five blocked cases pass; complete Ubuntu Sending has 40
  QtTest passes, zero failures/skips; focused Ubuntu CTest 5/5. Windows Sending,
  credential regression and ARM64 real-provider isolation checks also pass.
- Native ARM64 M2.45E ran 14 selected Debug registrations, including one skipped
  private replay case. Later native IDE and retirement checks are additional
  focused evidence, not proof of a complete Release CTest run.

Raw IDE logs, screenshots, private replay/capture evidence and later H/I reports
are owner-retained, ignored evidence, not files promised in a fresh clone. This
dated summary records their accepted scope. Older reports may still say acceptance
was pending or describe subsequently superseded limits; preserve their original
claims. Optional replay/live checks and platform-dependent skipped cases are not
qualification passes. No aggregate full-suite or all-instrument hardware pass is claimed.

## Outstanding work and maintenance

- The accepted Ubuntu checkout retains its historical name; no relocation or Qt
  Creator reset is required. Package qualification uses verified native dependency
  stages. Machine-local overrides outside those qualified build caches retain
  their own verification boundary.
- [#18](https://github.com/RFStateSide/RayNmeaSimulator-Archive/issues/18) now has a qualified
  private [Deploy workflow](../deployment/README.md), Windows/Linux runtime staging,
  installer preservation and ARM64/Debian 12 compatibility evidence. The original
  ARM64 glibc 2.38 failure and separate Debian 12 success remain distinct. Public
  distribution is still NO-GO; see the sole [public-release audit](../PUBLIC_RELEASE_AUDIT.md).
- Additional optional VesselMovement navigation mappings in
  [#29](https://github.com/RFStateSide/RayNmeaSimulator-Archive/issues/29) are explicitly
  deferred to v0.2.0. Other enhancements and hardware qualifications retain their
  existing issue scope and version decisions; no new release commitments are made.
- [#12](https://github.com/RFStateSide/RayNmeaSimulator-Archive/issues/12) tracks focused test
  discovery and current capability/qualification documentation.

Maintain this page when supported behavior, qualification or limitations change;
link dated evidence and identify which newer acceptance supersedes older wording.
Keep commands in the detailed guides, test inventory in CMake, and historical
reports unchanged. Never add private vessel data, routes, credentials or captures
to this summary.

Historical evidence links target the private RayNmeaSimulator-Archive repository;
they are not required to follow the current build and test instructions.
