# Security and privacy boundaries

Do not include credentials, access keys, private NMEA captures, vessel identity,
locations or owner settings in public reports. Coordinate sensitive disclosures
privately with the project owner through https://www.rfstateside.com/ rather than
posting recovered values in a public issue. Publication remains an owner decision.

## Operational boundaries

- External instrument transmission requires explicit Enable Sending. Route-service
  and device discovery have separate bounded authorization. Stop/F12 revokes TX.
- Viewer does not automatically authorize instrument Sending. Actisense Release
  remains RX-only; Debug transmission remains experimental.
- The optional TCP CAN bridge uses bounded sessions/queues and HMAC TX authorization.
  It is not TLS: CAN traffic and identities are visible on the network. Default
  bind is loopback; use an authenticated SSH tunnel on untrusted networks and
  do not expose the listener directly to the Internet. RX access is governed by
  its deployment/allowlist policy, not the TX key alone.
- Signal K credentials belong in secure OS storage. Use HTTPS outside a trusted
  laboratory network. New configurations default to loopback; explicit saved
  endpoints are preserved. URL validation rejects embedded credentials.
- Application logs redact credentials/URLs; NMEA Traffic recording is separate,
  explicit and privacy-sensitive. Review exports before sharing.
- GPX/KML import rejects DTD/entities and bounds bytes, nesting, elements and
  waypoint count. Layouts, Templates and external QSS are operator-selected local
  content, not a general untrusted-code sandbox. QSS can reference local assets.
- Debug bundled Template authoring is source editing and is unavailable in Release.
  Debug test menus are hidden in Release. Experimental environment diagnostics
  remain opt-in and must not be enabled in distributed support instructions.

## Release review

A heuristic scan is not proof that no secret exists. Review findings and history,
not only the current tree. Any confirmed exposed credential requires owner-managed
revocation/rotation and separately authorized history remediation; never test a
recovered credential against a provider. No Git history was rewritten in M2.50.

Historical artwork permissions, development reports, author metadata and packaged
third-party notices require explicit publication review. A current-tree cleanup
does not authorize public visibility or an installer. Use a clean source checkout
and an explicit package manifest; exclude owner Templates, captures and profiles.

The M2.50 private engineering evidence records scan scope and limitations. No
independent penetration-test or universal security certification is claimed.

Legacy EncryptDecrypt is an unused XOR-obfuscation prototype, not encryption,
and is excluded from active CMake targets. Do not use it to protect credentials.
