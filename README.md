# RayNmeaSimulator

RF StateSide's Qt desktop NMEA2000/NMEA0183 engineering simulator and Viewer.

Start with [current capabilities, platform support and qualification evidence](docs/capabilities-and-support.md).
This is the maintained status reference; historical reports retain their original scope.

## Build and develop

CMake/CTest is the supported project build system. The CMake-only migration and
disposable Linux credential-test isolation are committed and owner accepted.
Start with
[fresh-clone provisioning and CMake](docs/build-cmake.md), then the
[Windows](docs/build-windows.md), [Ubuntu](docs/build-linux.md) or
[Raspberry Pi ARM64](docs/build-raspberry-pi.md) guide.
[Pinned dependency setup](docs/DEPENDENCIES.txt) is explicit; normal configure/build
verifies existing installations without downloading dependencies.
[Focused tests and isolated helpers](tests/README.md) use existing CMake targets;
discovery does not mean every suite has been built or qualified.

## Build installation packages

Start with [deployment/README.md](deployment/README.md), then the practical
[Windows](deployment/windows/README.md) or [Linux/ARM64](deployment/linux/README.md)
instructions. In Qt Creator select **MinGW Release**, refresh CMake, select the
**Deploy** build target and build it. The selected build directory contains
`deploy/RayNmeaSimulator/` for inspection and `deploy/installer/` for the private
installer/archive and SHA-256 sidecar. Packages remain private development
candidates; public-release gates are still open.

Normal external instrument transmission requires explicit **Enable Sending**;
Stop/F12, mode and transport safeguards remain. Layout/Template loading never
authorizes Sending. Generator sources are optional for manual-property TX.
Transport restrictions, separately authorized protocol services and tested hardware
boundaries are described in the capability reference.

Required assets and curated Templates are embedded in Qt resources. No manual
Data/stylesheets copy is needed for development. See
[runtime resources](docs/RUNTIME-RESOURCES.txt) and
[development validation](docs/development-validation.md).
Historical reports retain their original findings; superseded build instructions
are preserved under [docs/history](https://github.com/RFStateSide/RayNmeaSimulator-Archive/blob/c8f2b5c822050ac875368a02b517e38a14a108b4/docs/history/README.md).

## License, provenance and security

The repository maintains one authoritative [public-release audit](PUBLIC_RELEASE_AUDIT.md).
Its publication gates remain open. Follow the [repository audit policy](AGENTS.md):
keep detailed milestone evidence under ignored `build/`, while maintaining normal
technical, deployment, Help, licensing/provenance, security and test documentation.

Original RayNmeaSimulator software is licensed under **AGPL-3.0-or-later**;
see [LICENSE](LICENSE). “Public Engineering Reference — AS-IS” is a project
classification, not a replacement license. Separately licensed components and
reference-derived material retain their notices; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

See [PROVENANCE.md](PROVENANCE.md), the [file inventory](docs/provenance-inventory.csv)
and [SECURITY.md](SECURITY.md) before preparing a distribution. Repository publication
and packaged-binary distribution require separate reviews. This documentation does
not announce a public release or certify that historical rights/privacy review is complete.

Historical evidence links target the private RayNmeaSimulator-Archive repository;
they are not required to follow the current build and test instructions.
