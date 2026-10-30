# Native macOS credentials

The macOS backend uses `SecItemAdd`, `SecItemCopyMatching`, `SecItemUpdate` and
`SecItemDelete`, linked through Security and CoreFoundation by macOS-only CMake
branches. Windows Credential Manager and Linux Secret Service remain unchanged.

## Contract and storage policy

The generic-password service is **RFStateSide.RayNmeaSimulator**. Account names
are the existing logical names (`SignalKToken`, `OpenWeatherMapApiKey`,
`NetworkNmea2000Token`). BrickSuite's **RFStateSide.BrickSuite** service is separate.
Queries always specify both service and account; there is no enumeration or
wildcard deletion. Empty writes remove; missing reads succeed with `found=false`;
missing removal succeeds. Duplicate adds replace through update, with failures
reported rather than treated as missing. Existing single-line / 2500 UTF-8-byte
validation remains in CredentialStore. Values are not trimmed by this backend.
CFData lengths preserve whitespace, Unicode and embedded NUL. Malformed return
types or invalid UTF-8 fail closed. Fixed error messages contain no credential
values or backend output. No plaintext fallback, subprocess storage or secret
arguments/environment/settings are introduced. Ordinary in-process QString /
CFData lifetime is retained; this is not a locked-memory secret container.

`kSecUseDataProtectionKeychain=false` deliberately chooses the file-based model
already used by BrickSuite. Normal production adds use the existing default
Keychain; lookup/update/delete use the existing search list. Neither is changed.
Native file-based ACL authorization and Keychain lock policy apply. Security may
present its normal authorization/unlock UI; denial/cancellation is reported.
No permissive trusted-app ACL is installed, and no accessibility/permission
setting is changed. No synchronizable item or shared access group is requested.
Do not claim `kSecAttrAccessibleWhenUnlockedThisDeviceOnly` or Data Protection
no-interaction flags apply to this file-based store: they do not.

Apple recommends Data Protection for new designs. Its access groups require
provisioned signing entitlements; that is a separate architectural/release
choice for this currently unsigned desktop build. File-based SecItem remains
supported, although Apple describes its implementation as on the road to
deprecation. There is no automatic fallback between stores. Future migration
must explicitly handle signing identity, existing credentials and acceptance.

## BrickSuite reference

Read-only reference: `src/services/CredentialStore.cpp`, its header, CMake links
and tests. BrickSuite uses deprecated `SecKeychainFindGenericPassword`,
`SecKeychainAddGenericPassword`, `SecKeychainItemModifyAttributesAndData`,
`SecKeychainItemDelete` and `SecKeychainItemFreeContent`. It uses exact service /
account strings and explicit UTF-8 lengths, reports OSStatus errors and releases
returned allocations. Its native branch does not log credential values, but
trims input. Its CMake links Security/CoreFoundation. Tests include Linux
secret-tool substitutes, dialog credential doubles and tests linked against the
native backend; no owned disposable macOS Keychain integration boundary was
found. They were not run or changed.

Reused: service/account design, native secure-store semantics, fixed application
identity, missing-is-success behavior and platform-specific framework linking.
Adapted: RayNmeaSimulator namespace and existing no-trim/empty-write/byte-limit
contract, current SecItem APIs, duplicate-update flow, RAII CF ownership,
redacted actionable errors and explicit test isolation. No BrickSuite credential
identifiers, entries, private data, settings, builds or deployment files are reused.

## Test boundaries

All ordinary macOS `RAYNMEA_TESTING` credentials link a compile-time in-memory
SecItem substitute. It exercises the same adapter and enforces service/account /
query policy, permits deterministic native-status failures and never calls real
item APIs. Test accounts retain the existing hashed-profile namespace. Their
persistence ends with the process; reconstructing MainWindow is a caller/client
lifecycle check, not proof of disk persistence. CTest credential regressions carry
`unit;credential-double` labels and backendName identifies the substitute.
Production `--isolated-profile` refuses all credential operations before any
Security item API call, even with a valid disposable settings directory. A test
subprocess verifies that guard. Namespacing alone never authorizes real storage.

Actual integration is a separate explicit opt-in target:

```sh
cmake -S . -B <qualified-build> -DRAYNMEA_ENABLE_MACOS_KEYCHAIN_INTEGRATION=ON
cmake --build <qualified-build> --target macos_keychain_integration_tests
ctest --test-dir <qualified-build> -R '^macos_keychain_integration_tests$' --output-on-failure
```

It creates a randomly passworded Keychain in an owned profile directory using
`SecKeychainCreate`, verifies its returned reference/path and unchanged user
search list/default BEFORE item operations, and injects **kSecUseKeychain** into
every add and a singleton **kSecMatchSearchList** into every read/update/delete.
The injection is test-only; production has no environment-selected alternate
store. The restart child reopens/unlocks only that same file; its path/password
travel over stdin, not arguments or environment. Only synthetic items in the
owned store are touched. Successful completion checks deletion and unchanged
configuration; a scope guard also attempts owned-reference cleanup on failures.
Failures are failures, never skips or substituted successes. A failed creation /
boundary check must stop item operations. Do not reroute failures to login.

Deprecated lifecycle APIs (`SecKeychainCreate/Open/Unlock/GetPath/Delete`) are
confined to this test, reviewed against the installed SDK and Apple's current
TN3137/API documentation and published Security implementation. The latter
explicitly excludes private Keychain creation from search-list changes
(`StorageManager::shouldAddToSearchList`, rdar://13529331). This is distinct from
`security create-keychain`, which adds to the search list and is prohibited here.
TN3137 also warns that macOS 26.4+ file Keychains can depend on protected entropy
files: do not assume a portable standalone file, copy system entropy, weaken SIP
or rely on undocumented storage paths. The test uses Security APIs in place.
This narrow fixture is engineering-qualified on macOS 26.7.1, not a promise about
future OS releases. On unsupported lifecycle behavior, stop and use a separately
approved disposable login account; HOME redirection alone is not isolation.

Real permission denial/cancellation, locked-store UI, different code identities,
Release/distribution signatures and full native GUI acceptance remain separate
owner validation. Ordinary Setup regressions use the substitute; actual Keychain
CRUD and separate-process persistence use the integration fixture. No ordinary
production application session or owner credential item is required for either.

Sources:
- [Apple TN3137](https://developer.apple.com/documentation/technotes/tn3137-on-mac-keychains)
- [SecKeychainCreate](https://developer.apple.com/documentation/security/seckeychaincreate(_:_:_:_:_:_:))
- [SecItemAdd](https://developer.apple.com/documentation/security/secitemadd(_:_:))
- [Apple Security StorageManager](https://github.com/apple-oss-distributions/Security/blob/main/OSX/libsecurity_keychain/lib/StorageManager.cpp)

The Ubuntu historical checkout naming and machine-local DEPS_ROOT remain
unresolved; this work does not address them. Official Qt accessibility maintenance
adoption remains tracked in private Issue #33; this backend does not qualify the
remaining Qt Creator or native accessibility behavior.
